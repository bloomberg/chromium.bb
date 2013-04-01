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
  if (sample_rate == 44100)
    buffer_size = 2 * 440;
  else
    buffer_size = 2 * sample_rate / 100;
#elif defined(OS_ANDROID)
  // TODO(leozwang): Tune and adjust buffer size on Android.
  if (sample_rate == 44100)
    buffer_size = 2 * 440;
  else
    buffer_size = 2 * sample_rate / 100;
#endif

  return buffer_size;
}

// Reference counted container of WebRtcAudioCapturerSink delegates.
class WebRtcAudioCapturer::SinkOwner
    : public base::RefCounted<WebRtcAudioCapturer::SinkOwner>,
      public WebRtcAudioCapturerSink {
 public:
  explicit SinkOwner(WebRtcAudioCapturerSink* sink);

  virtual void CaptureData(const int16* audio_data,
                           int number_of_channels,
                           int number_of_frames,
                           int audio_delay_milliseconds,
                           double volume) OVERRIDE;
  virtual void SetCaptureFormat(const media::AudioParameters& params) OVERRIDE;

  bool IsEqual(const WebRtcAudioCapturerSink* other) const;
  void Reset();

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct WrapsSink {
    WrapsSink(WebRtcAudioCapturerSink* sink) : sink_(sink) {}
    bool operator()(
        const scoped_refptr<WebRtcAudioCapturer::SinkOwner>& owner) {
      return owner->IsEqual(sink_);
    }
    WebRtcAudioCapturerSink* sink_;
  };

 private:
  virtual ~SinkOwner() {}

  friend class base::RefCounted<WebRtcAudioCapturer::SinkOwner>;
  WebRtcAudioCapturerSink* delegate_;
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(SinkOwner);
};

WebRtcAudioCapturer::SinkOwner::SinkOwner(
    WebRtcAudioCapturerSink* sink)
    : delegate_(sink) {
}

void WebRtcAudioCapturer::SinkOwner::CaptureData(
    const int16* audio_data, int number_of_channels, int number_of_frames,
    int audio_delay_milliseconds, double volume) {
  base::AutoLock lock(lock_);
  if (delegate_) {
    delegate_->CaptureData(audio_data, number_of_channels, number_of_frames,
                           audio_delay_milliseconds, volume);
  }
}

void WebRtcAudioCapturer::SinkOwner::SetCaptureFormat(
    const media::AudioParameters& params) {
  base::AutoLock lock(lock_);
  if (delegate_)
    delegate_->SetCaptureFormat(params);
}

bool WebRtcAudioCapturer::SinkOwner::IsEqual(
    const WebRtcAudioCapturerSink* other) const {
  base::AutoLock lock(lock_);
  return (other == delegate_);
}

void WebRtcAudioCapturer::SinkOwner::Reset() {
  base::AutoLock lock(lock_);
  delegate_ = NULL;
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
    int channels = ChannelLayoutToChannelCount(channel_layout);
    params_.Reset(format, channel_layout, channels, 0,
        sample_rate, bits_per_sample, buffer_size);
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
                                     int sample_rate,
                                     int session_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sinks_.empty());
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";

  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);

  session_id_ = session_id;

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
  // providing an alternative media::AudioCapturerSource.
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(),
                    channel_layout,
                    static_cast<float>(sample_rate));

  return true;
}

WebRtcAudioCapturer::WebRtcAudioCapturer()
    : source_(NULL),
      running_(false),
      agc_is_enabled_(false),
      session_id_(0) {
  DVLOG(1) << "WebRtcAudioCapturer::WebRtcAudioCapturer()";
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sinks_.empty());
  DCHECK(!running_);
  DVLOG(1) << "WebRtcAudioCapturer::~WebRtcAudioCapturer()";
}

void WebRtcAudioCapturer::AddCapturerSink(WebRtcAudioCapturerSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::AddCapturerSink()";
  base::AutoLock auto_lock(lock_);
  // Verify that |sink| is not already added to the list.
  DCHECK(std::find_if(
      sinks_.begin(), sinks_.end(), SinkOwner::WrapsSink(sink)) ==
      sinks_.end());
  // Create (and add to the list) a new SinkOwner which owns the |sink|
  // and delagates all calls to the WebRtcAudioCapturerSink interface.
  sinks_.push_back(new WebRtcAudioCapturer::SinkOwner(sink));
}

void WebRtcAudioCapturer::RemoveCapturerSink(WebRtcAudioCapturerSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::RemoveCapturerSink()";

  base::AutoLock auto_lock(lock_);

  // Get iterator to the first element for which WrapsSink(sink) returns true.
  SinkList::iterator it = std::find_if(sinks_.begin(), sinks_.end(),
                                       SinkOwner::WrapsSink(sink));
  if (it != sinks_.end()) {
    // Clear the delegate to ensure that no more capture callbacks will
    // be sent to this sink. Also avoids a possible crash which can happen
    // if this method is called while capturing is active.
    (*it)->Reset();
    sinks_.erase(it);
  }
}

void WebRtcAudioCapturer::SetCapturerSource(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::ChannelLayout channel_layout,
    float sample_rate) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
    source->Initialize(current_buffer->params(), this, session_id_);
  }
}

void WebRtcAudioCapturer::Start() {
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_)
    return;

  // Start the data source, i.e., start capturing data from the current source.
  // Note that, the source does not have to be a microphone.
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

void WebRtcAudioCapturer::SetAutomaticGainControl(bool enable) {
  base::AutoLock auto_lock(lock_);
  // Store the setting since SetAutomaticGainControl() can be called before
  // Initialize(), in this case stored setting will be applied in Start().
  agc_is_enabled_ = enable;

  if (source_)
    source_->SetAutomaticGainControl(enable);
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

media::AudioParameters WebRtcAudioCapturer::audio_parameters() const {
  base::AutoLock auto_lock(lock_);
  return buffer_->params();
}

}  // namespace content
