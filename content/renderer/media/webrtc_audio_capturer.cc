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
        "Sample rate not supported";
  }
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 10ms on the
  // output side.
  // TODO(henrika): It might be possible to reduce the input buffer
  // size and reduce the delay even more.
  buffer_size = 2 * sample_rate / 100;
#elif defined(OS_ANDROID)
  // TODO(leozwang): Tune and adjust buffer size on Android.
  buffer_size = 2 * sample_rate / 100;
#endif

  return buffer_size;
}

// This is a temporary audio buffer with parameters used to send data to
// callbacks.
class WebRtcAudioCapturer::ConfiguredBuffer :
    public base::RefCounted<WebRtcAudioCapturer::ConfiguredBuffer> {
 public:
  ConfiguredBuffer() {}

  bool Initialize(int sample_rate,
                  media::ChannelLayout channel_layout) {
    int buffer_size = GetBufferSizeForSampleRate(sample_rate);
    if (!buffer_size) {
      DLOG(ERROR) << "Unsupported sample-rate: " << sample_rate;
      return false;
    }

    media::AudioParameters::Format format =
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY;

    // bits_per_sample is always 16 for now.
    int bits_per_sample = 16;

    params_.Reset(format, channel_layout, 0, sample_rate, bits_per_sample,
                  buffer_size);
    buffer_.reset(new int16[params_.frames_per_buffer() * params_.channels()]);

    return true;
  }

  int16* buffer() const { return buffer_.get(); }
  const media::AudioParameters& params() const { return params_; }

 private:
  ~ConfiguredBuffer() {}
  friend class base::RefCounted<WebRtcAudioCapturer::ConfiguredBuffer>;

  scoped_ptr<int16[]> buffer_;

  // Cached values of utilized audio parameters.
  media::AudioParameters params_;
};

// static
scoped_refptr<WebRtcAudioCapturer> WebRtcAudioCapturer::CreateCapturer() {
  scoped_refptr<WebRtcAudioCapturer> capturer = new  WebRtcAudioCapturer();
  return capturer;
}

bool WebRtcAudioCapturer::Reconfigure(int sample_rate,
                                      media::ChannelLayout channel_layout) {
  scoped_refptr<ConfiguredBuffer> new_buffer(new ConfiguredBuffer());
  if (!new_buffer->Initialize(sample_rate, channel_layout))
    return false;

  SinkList sinks;
  {
    base::AutoLock auto_lock(lock_);

    buffer_ = new_buffer;
    sinks = sinks_;
  }

  // Tell all sinks which format we use.
  for (SinkList::const_iterator it = sinks.begin(); it != sinks.end(); ++it)
    (*it)->SetCaptureFormat(new_buffer->params());

  return true;
}

bool WebRtcAudioCapturer::Initialize(media::ChannelLayout channel_layout,
                                     int sample_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sinks_.empty());
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";

  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);

  // Verify that the reported input channel configuration is supported.
  if (channel_layout != media::CHANNEL_LAYOUT_MONO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO) {
    DLOG(ERROR) << channel_layout
                << " is not a supported input channel configuration.";
    return false;
  }

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

  if (!Reconfigure(sample_rate, channel_layout))
    return false;

  // Create and configure the default audio capturing source. The |source_|
  // will be overwritten if an external client later calls SetCapturerSource()
  // providing an alternaive media::AudioCapturerSource.
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(),
                    channel_layout,
                    static_cast<float>(sample_rate));

  return true;
}

WebRtcAudioCapturer::WebRtcAudioCapturer()
    : source_(NULL),
      running_(false),
      buffering_(false),
      agc_is_enabled_(false) {
  DVLOG(1) << "WebRtcAudioCapturer::WebRtcAudioCapturer()";
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sinks_.empty());
  DCHECK(!loopback_fifo_);
  DVLOG(1) << "WebRtcAudioCapturer::~WebRtcAudioCapturer()";
}

void WebRtcAudioCapturer::AddCapturerSink(WebRtcAudioCapturerSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::AddCapturerSink()";
  base::AutoLock auto_lock(lock_);
  DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
}

void WebRtcAudioCapturer::RemoveCapturerSink(WebRtcAudioCapturerSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::RemoveCapturerSink()";
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
  DVLOG(1) << "SetCapturerSource(channel_layout=" << channel_layout << ","
           << "sample_rate=" << sample_rate << ")";
  scoped_refptr<media::AudioCapturerSource> old_source;
  scoped_refptr<ConfiguredBuffer> current_buffer;
  {
    base::AutoLock auto_lock(lock_);
    if (source_ == source)
      return;

    source_.swap(old_source);
    source_ = source;
    current_buffer = buffer_;
  }

  const bool no_default_audio_source_exists = !current_buffer;

  // Detach the old source from normal recording or perform first-time
  // initialization if Initialize() has never been called. For the second
  // case, the caller is not "taking over an ongoing session" but instead
  // "taking control over a new session".
  if (old_source || no_default_audio_source_exists) {
    DVLOG(1) << "New capture source will now be utilized.";
    if (old_source)
      old_source->Stop();

    // Dispatch the new parameters both to the sink(s) and to the new source.
    // The idea is to get rid of any dependency of the microphone parameters
    // which would normally be used by default.
    if (!Reconfigure(sample_rate, channel_layout)) {
      return;
    } else {
      // The buffer has been reconfigured.  Update |current_buffer|.
      base::AutoLock auto_lock(lock_);
      current_buffer = buffer_;
    }
  }

  if (source) {
    // Make sure to grab the new parameters in case they were reconfigured.
    source->Initialize(current_buffer->params(), this, this);
  }
}

void WebRtcAudioCapturer::SetStopCallback(
    const base::Closure& on_device_stopped_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) <<  "WebRtcAudioCapturer::SetStopCallback()";
  base::AutoLock auto_lock(lock_);
  on_device_stopped_cb_ = on_device_stopped_cb;
}

void WebRtcAudioCapturer::PrepareLoopback() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  loopback_fifo_.reset(new media::AudioFifo(
      buffer_->params().channels(),
      10 * buffer_->params().frames_per_buffer()));
  buffering_ = true;
}

void WebRtcAudioCapturer::CancelLoopback() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) <<  "WebRtcAudioCapturer::CancelLoopback()";
  base::AutoLock auto_lock(lock_);
  buffering_ = false;
  if (loopback_fifo_.get() != NULL) {
    loopback_fifo_->Clear();
    loopback_fifo_.reset();
  }
}

void WebRtcAudioCapturer::PauseBuffering() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) <<  "WebRtcAudioCapturer::PauseBuffering()";
  base::AutoLock auto_lock(lock_);
  buffering_ = false;
}

void WebRtcAudioCapturer::ResumeBuffering() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) <<  "WebRtcAudioCapturer::ResumeBuffering()";
  base::AutoLock auto_lock(lock_);
  if (buffering_)
    return;
  if (loopback_fifo_.get() != NULL)
    loopback_fifo_->Clear();
  buffering_ = true;
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
    DVLOG(2) << "WARNING: loopback FIFO is empty.";
  }
}

void WebRtcAudioCapturer::Start() {
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_)
    return;

  // What Start() and SetAutomaticGainControl() does is supposed to be very
  // light, for example, posting a task to another thread, so it is safe to
  // call Start() and SetAutomaticGainControl() under the lock.
  if (source_) {
    // We need to set the AGC control before starting the stream.
    source_->SetAutomaticGainControl(agc_is_enabled_);
    source_->Start();
  }

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
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::SetDevice(" << session_id << ")";
  base::AutoLock auto_lock(lock_);
  if (source_)
    source_->SetDevice(session_id);
}

void WebRtcAudioCapturer::SetAutomaticGainControl(bool enable) {
  base::AutoLock auto_lock(lock_);
  // Store the setting since SetAutomaticGainControl() can be called before
  // Initialize(), in this case stored setting will be applied in Start().
  agc_is_enabled_ = enable;

  if (source_)
    source_->SetAutomaticGainControl(enable);
}

bool WebRtcAudioCapturer::IsInLoopbackMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  scoped_refptr<ConfiguredBuffer> buffer_ref_while_calling;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Copy the stuff we will need to local variables. In particular, we grab
    // a reference to the buffer so we can ensure it stays alive even if the
    // buffer is reconfigured while we are calling back.
    buffer_ref_while_calling = buffer_;
    sinks = sinks_;

    // Push captured audio to FIFO so it can be read by a local sink.
    // Buffering is only enabled if we are rendering a local media stream.
    if (loopback_fifo_ && buffering_) {
      if (loopback_fifo_->frames() + audio_source->frames() <=
          loopback_fifo_->max_frames()) {
        loopback_fifo_->Push(audio_source);
      } else {
        DVLOG(1) << "FIFO is full";
      }
    }
  }

  int bytes_per_sample =
      buffer_ref_while_calling->params().bits_per_sample() / 8;

  // Interleave, scale, and clip input to int and store result in
  // a local byte buffer.
  audio_source->ToInterleaved(audio_source->frames(), bytes_per_sample,
                              buffer_ref_while_calling->buffer());

  // Feed the data to the sinks.
  for (SinkList::const_iterator it = sinks.begin();
       it != sinks.end();
       ++it) {
    (*it)->CaptureData(buffer_ref_while_calling->buffer(),
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

media::AudioParameters WebRtcAudioCapturer::audio_parameters() const {
  base::AutoLock auto_lock(lock_);
  return buffer_->params();
}

}  // namespace content
