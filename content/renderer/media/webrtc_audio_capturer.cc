// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "content/common/child_process.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_renderer.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"

namespace content {

// Supported hardware sample rates for input and output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// media::GetAudioInputHardwareSampleRate() asks the audio layer
// for its current sample rate (set by the user) on Windows and Mac OS X.
// The listed rates below adds restrictions and WebRtcAudioDeviceImpl::Init()
// will fail if the user selects any rate outside these ranges.
static int kValidInputRates[] = {96000, 48000, 44100, 32000, 16000, 8000};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
static int kValidInputRates[] = {48000, 44100};
#elif defined(OS_ANDROID)
static int kValidInputRates[] = {48000, 44100, 16000};
#else
static int kValidInputRates[] = {44100};
#endif

static int GetBufferSizeForSampleRate(int sample_rate) {
  int buffer_size = 0;
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Use different buffer sizes depending on the current hardware sample rate.
  if (sample_rate == 44100) {
    // We do run at 44.1kHz at the actual audio layer, but ask for frames
    // at 44.0kHz to ensure that we can feed them to the webrtc::VoiceEngine.
    buffer_size = 440;
  } else {
    buffer_size = (sample_rate / 100);
    DCHECK_EQ(buffer_size * 100, sample_rate) <<
        "Sample rate not supported. Should have been caught in Init().";
  }
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 10ms on the
  // output side.
  // TODO(henrika): It might be possible to reduce the input buffer
  // size and reduce the delay even more.
  buffer_size = 2 * sample_rate / 100;
#endif

  return buffer_size;
}

// static
scoped_refptr<WebRtcAudioCapturer> WebRtcAudioCapturer::CreateCapturer() {
  scoped_refptr<WebRtcAudioCapturer> capturer = new  WebRtcAudioCapturer();
  if (capturer->Initialize())
    return capturer;

  return NULL;
}

WebRtcAudioCapturer::WebRtcAudioCapturer()
    : source_(NULL),
      running_(false),
      buffering_(false) {
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(sinks_.empty());
  DCHECK(!loopback_fifo_);
}

void WebRtcAudioCapturer::AddCapturerSink(WebRtcAudioCapturerSink* sink) {
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
    sinks_.push_back(sink);
  }

  // Tell the |sink| which format we use.
  sink->SetCaptureFormat(params_);
}

void WebRtcAudioCapturer::RemoveCapturerSink(WebRtcAudioCapturerSink* sink) {
  base::AutoLock auto_lock(lock_);
  for (SinkList::iterator it = sinks_.begin(); it != sinks_.end(); ++it) {
    if (sink == *it) {
      sinks_.erase(it);
      break;
    }
  }
}

void WebRtcAudioCapturer::SetCapturerSource(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::ChannelLayout channel_layout,
    float sample_rate) {
  DVLOG(1) <<  "SetCapturerSource()";
  scoped_refptr<media::AudioCapturerSource> old_source;
  {
    base::AutoLock auto_lock(lock_);
    if (source_ == source)
      return;

    source_.swap(old_source);
    source_ = source;
  }

  // Detach the old source from normal recording.
  if (old_source) {
    old_source->Stop();

    // Dispatch the new parameters both to the sink(s) and to the new source.
    // The idea is to get rid of any dependency of the microphone parameters
    // which would normally be used by default.

    int buffer_size = GetBufferSizeForSampleRate(sample_rate);
    if (!buffer_size) {
      DLOG(ERROR) << "Unsupported sample-rate: " << sample_rate;
      return;
    }

    params_.Reset(params_.format(),
                  channel_layout,
                  sample_rate,
                  16,  // ignored since the audio stack uses float32.
                  buffer_size);

    buffer_.reset(new int16[params_.frames_per_buffer() * params_.channels()]);

    for (SinkList::const_iterator it = sinks_.begin();
         it != sinks_.end(); ++it) {
      (*it)->SetCaptureFormat(params_);
    }
  }

  if (source)
    source->Initialize(params_, this, this);
}

void WebRtcAudioCapturer::SetStopCallback(
    const base::Closure& on_device_stopped_cb) {
  DVLOG(1) <<  "WebRtcAudioCapturer::SetStopCallback()";
  base::AutoLock auto_lock(lock_);
  on_device_stopped_cb_ = on_device_stopped_cb;
}

void WebRtcAudioCapturer::PrepareLoopback() {
  DVLOG(1) <<  "WebRtcAudioCapturer::PrepareLoopback()";
  base::AutoLock auto_lock(lock_);
  DCHECK(!loopback_fifo_);

  // TODO(henrika): we could add a more dynamic solution here but I prefer
  // a fixed size combined with bad audio at overflow. The alternative is
  // that we start to build up latency and that can be more difficult to
  // detect. Tests have shown that the FIFO never contains more than 2 or 3
  // audio frames but I have selected a max size of ten buffers just
  // in case since these tests were performed on a 16 core, 64GB Win 7
  // machine. We could also add some sort of error notifier in this area if
  // the FIFO overflows.
  loopback_fifo_.reset(new media::AudioFifo(params_.channels(),
                       10 * params_.frames_per_buffer()));
  buffering_ = true;
}

void WebRtcAudioCapturer::CancelLoopback() {
  DVLOG(1) <<  "WebRtcAudioCapturer::CancelLoopback()";
  base::AutoLock auto_lock(lock_);
  buffering_ = false;
  if (loopback_fifo_.get() != NULL) {
    loopback_fifo_->Clear();
    loopback_fifo_.reset();
  }
}

void WebRtcAudioCapturer::PauseBuffering() {
  DVLOG(1) <<  "WebRtcAudioCapturer::PauseBuffering()";
  base::AutoLock auto_lock(lock_);
  buffering_ = false;
}

void WebRtcAudioCapturer::ResumeBuffering() {
  DVLOG(1) <<  "WebRtcAudioCapturer::ResumeBuffering()";
  base::AutoLock auto_lock(lock_);
  if (buffering_)
    return;
  if (loopback_fifo_.get() != NULL)
    loopback_fifo_->Clear();
  buffering_ = true;
}

bool WebRtcAudioCapturer::Initialize() {
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";
  // Ask the browser for the default audio input hardware sample-rate.
  // This request is based on a synchronous IPC message.
  // TODO(xians): we should ask for the native sample rate of a specific device.
  int sample_rate = GetAudioInputSampleRate();
  DVLOG(1) << "Audio input hardware sample rate: " << sample_rate;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputSampleRate",
                            sample_rate, media::kUnexpectedAudioSampleRate);

  // Verify that the reported input hardware sample rate is supported
  // on the current platform.
  if (std::find(&kValidInputRates[0],
                &kValidInputRates[0] + arraysize(kValidInputRates),
                sample_rate) ==
          &kValidInputRates[arraysize(kValidInputRates)]) {
    DLOG(ERROR) << sample_rate << " is not a supported input rate.";
    return false;
  }

  // Ask the browser for the default number of audio input channels.
  // This request is based on a synchronous IPC message.
  // TODO(xians): we should ask for the layout of a specific device.
  media::ChannelLayout channel_layout = GetAudioInputChannelLayout();
  DVLOG(1) << "Audio input hardware channels: " << channel_layout;

  media::AudioParameters::Format format =
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
  int buffer_size = GetBufferSizeForSampleRate(sample_rate);
  if (!buffer_size) {
    DLOG(ERROR) << "Unsupported platform";
    return false;
  }

  params_.Reset(format, channel_layout, sample_rate, 16, buffer_size);

  buffer_.reset(new int16[params_.frames_per_buffer() * params_.channels()]);

  // Create and configure the default audio capturing source. The |source_|
  // will be overwritten if the client call the source calls
  // SetCapturerSource().
  SetCapturerSource(
      AudioDeviceFactory::NewInputDevice(), channel_layout, sample_rate);

  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);

  return true;
}

void WebRtcAudioCapturer::ProvideInput(media::AudioBus* dest) {
  base::AutoLock auto_lock(lock_);
  DCHECK(loopback_fifo_.get() != NULL);

  if (!running_) {
    dest->Zero();
    return;
  }

  // Provide data by reading from the FIFO if the FIFO contains enough
  // to fulfill the request.
  if (loopback_fifo_->frames() >= dest->frames()) {
    loopback_fifo_->Consume(dest, 0, dest->frames());
  } else {
    dest->Zero();
    // This warning is perfectly safe if it happens for the first audio
    // frames. It should not happen in a steady-state mode.
    DLOG(WARNING) << "WARNING: loopback FIFO is empty.";
  }
}

void WebRtcAudioCapturer::Start() {
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_)
    return;

  // What Start() does is supposed to be very light, for example, posting a
  // task to another thread, so it is safe to call Start() under the lock.
  if (source_)
    source_->Start();

  running_ = true;
}

void WebRtcAudioCapturer::Stop() {
  DVLOG(1) << "WebRtcAudioCapturer::Stop()";
  scoped_refptr<media::AudioCapturerSource> source;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Ignore the Stop() request if we need to continue running for the
    // local capturer.
    if (loopback_fifo_) {
      loopback_fifo_->Clear();
      return;
    }

    source = source_;
    running_ = false;
  }

  if (source)
    source->Stop();
}

void WebRtcAudioCapturer::SetVolume(double volume) {
  DVLOG(1) << "WebRtcAudioCapturer::SetVolume()";
  base::AutoLock auto_lock(lock_);

  if (source_)
    source_->SetVolume(volume);
}

void WebRtcAudioCapturer::SetDevice(int session_id) {
  DVLOG(1) << "WebRtcAudioCapturer::SetDevice(" << session_id << ")";
  base::AutoLock auto_lock(lock_);
  if (source_)
    source_->SetDevice(session_id);
}

void WebRtcAudioCapturer::SetAutomaticGainControl(bool enable) {
  base::AutoLock auto_lock(lock_);
  if (source_)
    source_->SetAutomaticGainControl(enable);
}

bool WebRtcAudioCapturer::IsInLoopbackMode() {
  base::AutoLock auto_lock(lock_);
  return (loopback_fifo_ != NULL);
}

void WebRtcAudioCapturer::Capture(media::AudioBus* audio_source,
                                  int audio_delay_milliseconds,
                                  double volume) {
  // This callback is driven by AudioInputDevice::AudioThreadCallback if
  // |source_| is AudioInputDevice, otherwise it is driven by client's
  // CaptureCallback.
  SinkList sinks;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Copy the sink list to a local variable.
    sinks = sinks_;

    // Push captured audio to FIFO so it can be read by a local sink.
    // Buffering is only enabled if we are rendering a local media stream.
    if (loopback_fifo_ && buffering_) {
      if (loopback_fifo_->frames() + audio_source->frames() <=
          loopback_fifo_->max_frames()) {
        loopback_fifo_->Push(audio_source);
      } else {
        DLOG(WARNING) << "FIFO is full";
      }
    }
  }

  // Interleave, scale, and clip input to int and store result in
  // a local byte buffer.
  audio_source->ToInterleaved(audio_source->frames(),
                              params_.bits_per_sample() / 8,
                              buffer_.get());

  // Feed the data to the sinks.
  for (SinkList::const_iterator it = sinks.begin();
       it != sinks.end();
       ++it) {
    (*it)->CaptureData(reinterpret_cast<const int16*>(buffer_.get()),
                       audio_source->channels(), audio_source->frames(),
                       audio_delay_milliseconds, volume);
  }
}

void WebRtcAudioCapturer::OnCaptureError() {
  NOTIMPLEMENTED();
}

void WebRtcAudioCapturer::OnDeviceStarted(const std::string& device_id) {
  device_id_ = device_id;
}

void WebRtcAudioCapturer::OnDeviceStopped() {
  DCHECK_EQ(MessageLoop::current(), ChildProcess::current()->io_message_loop());
  DVLOG(1) << "WebRtcAudioCapturer::OnDeviceStopped()";
  {
    base::AutoLock auto_lock(lock_);
    running_ = false;
    buffering_ = false;
    if (loopback_fifo_) {
      loopback_fifo_->Clear();
    }
  }

  // Inform the local renderer about the stopped device.
  // The renderer can then save resources by not asking for more data from
  // the stopped source. We are on the IO thread but the callback task will
  // be posted on the message loop of the main render thread thanks to
  // usage of BindToLoop() when the callback was initialized.
  if (!on_device_stopped_cb_.is_null())
    on_device_stopped_cb_.Run();
}

}  // namespace content
