// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/audio/audio_util.h"
#include "media/audio/sample_rates.h"

namespace content {

namespace {

// Supported hardware sample rates for input and output sides.
#if defined(OS_WIN) || defined(OS_MACOSX)
// media::GetAudioInputHardwareSampleRate() asks the audio layer
// for its current sample rate (set by the user) on Windows and Mac OS X.
// The listed rates below adds restrictions and WebRtcAudioDeviceImpl::Init()
// will fail if the user selects any rate outside these ranges.
const int kValidInputRates[] = {96000, 48000, 44100, 32000, 16000, 8000};
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
const int kValidInputRates[] = {48000, 44100};
#elif defined(OS_ANDROID)
const int kValidInputRates[] = {48000, 44100};
#else
const int kValidInputRates[] = {44100};
#endif

int GetBufferSizeForSampleRate(int sample_rate) {
  int buffer_size = 0;
#if defined(OS_WIN) || defined(OS_MACOSX)
  // Use a buffer size of 10ms.
  buffer_size = (sample_rate / 100);
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // Based on tests using the current ALSA implementation in Chrome, we have
  // found that the best combination is 20ms on the input side and 10ms on the
  // output side.
  buffer_size = 2 * sample_rate / 100;
#elif defined(OS_ANDROID)
  // TODO(leozwang): Tune and adjust buffer size on Android.
    buffer_size = 2 * sample_rate / 100;
#endif
  return buffer_size;
}

}  // namespace

// This is a temporary audio buffer with parameters used to send data to
// callbacks.
class WebRtcAudioCapturer::ConfiguredBuffer :
    public base::RefCounted<WebRtcAudioCapturer::ConfiguredBuffer> {
 public:
  ConfiguredBuffer() {}

  bool Initialize(int sample_rate,
                  media::ChannelLayout channel_layout) {
    int buffer_size = GetBufferSizeForSampleRate(sample_rate);
    DVLOG(1) << "Using WebRTC input buffer size: " << buffer_size;

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

// Reference counted container of WebRtcLocalAudioTrack delegate.
class WebRtcAudioCapturer::TrackOwner
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer::TrackOwner> {
 public:
  explicit TrackOwner(WebRtcLocalAudioTrack* track)
      : delegate_(track) {}

  void CaptureData(const int16* audio_data,
                   int number_of_channels,
                   int number_of_frames,
                   int audio_delay_milliseconds,
                   int volume) {
    base::AutoLock lock(lock_);
    if (delegate_) {
      delegate_->CaptureData(audio_data,
                             number_of_channels,
                             number_of_frames,
                             audio_delay_milliseconds,
                             volume);
    }
  }

  void SetCaptureFormat(const media::AudioParameters& params) {
    base::AutoLock lock(lock_);
    if (delegate_)
      delegate_->SetCaptureFormat(params);
  }

  void Reset() {
    base::AutoLock lock(lock_);
    delegate_ = NULL;
  }

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct TrackWrapper {
    TrackWrapper(WebRtcLocalAudioTrack* track) : track_(track) {}
    bool operator()(
        const scoped_refptr<WebRtcAudioCapturer::TrackOwner>& owner) {
      return owner->IsEqual(track_);
    }
    WebRtcLocalAudioTrack* track_;
  };

 protected:
  virtual ~TrackOwner() {}

 private:
  friend class base::RefCountedThreadSafe<WebRtcAudioCapturer::TrackOwner>;

  bool IsEqual(const WebRtcLocalAudioTrack* other) const {
    base::AutoLock lock(lock_);
    return (other == delegate_);
  }

  // Do NOT reference count the |delegate_| to avoid cyclic reference counting.
  WebRtcLocalAudioTrack* delegate_;
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(TrackOwner);
};

// static
scoped_refptr<WebRtcAudioCapturer> WebRtcAudioCapturer::CreateCapturer() {
  scoped_refptr<WebRtcAudioCapturer> capturer = new WebRtcAudioCapturer();
  return capturer;
}

bool WebRtcAudioCapturer::Reconfigure(int sample_rate,
                                      media::ChannelLayout channel_layout) {
  scoped_refptr<ConfiguredBuffer> new_buffer(new ConfiguredBuffer());
  if (!new_buffer->Initialize(sample_rate, channel_layout))
    return false;

  TrackList tracks;
  {
    base::AutoLock auto_lock(lock_);

    buffer_ = new_buffer;
    tracks = tracks_;
  }

  // Tell all audio_tracks which format we use.
  for (TrackList::const_iterator it = tracks.begin();
       it != tracks.end(); ++it)
    (*it)->SetCaptureFormat(new_buffer->params());

  return true;
}

bool WebRtcAudioCapturer::Initialize(int render_view_id,
                                     media::ChannelLayout channel_layout,
                                     int sample_rate,
                                     int session_id,
                                     const std::string& device_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";

  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);

  session_id_ = session_id;
  device_id_ = device_id;
  if (render_view_id == -1) {
    // This capturer is used by WebAudio, return true without creating a
    // default capturing source. WebAudio will inject its own source via
    // SetCapturerSource() at a later state.
    DCHECK(device_id.empty());
    return true;
  }

  // Verify that the reported input channel configuration is supported.
  if (channel_layout != media::CHANNEL_LAYOUT_MONO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO) {
    DLOG(ERROR) << channel_layout
                << " is not a supported input channel configuration.";
    return false;
  }

  DVLOG(1) << "Audio input hardware sample rate: " << sample_rate;
  media::AudioSampleRate asr = media::AsAudioSampleRate(sample_rate);
  if (asr != media::kUnexpectedAudioSampleRate) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebRTC.AudioInputSampleRate", asr, media::kUnexpectedAudioSampleRate);
  } else {
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioInputSampleRateUnexpected", sample_rate);
  }

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
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(render_view_id),
                    channel_layout,
                    static_cast<float>(sample_rate));

  return true;
}

WebRtcAudioCapturer::WebRtcAudioCapturer()
    : source_(NULL),
      running_(false),
      agc_is_enabled_(false),
      session_id_(0),
      volume_(0) {
  DVLOG(1) << "WebRtcAudioCapturer::WebRtcAudioCapturer()";
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(tracks_.empty());
  DCHECK(!running_);
  DVLOG(1) << "WebRtcAudioCapturer::~WebRtcAudioCapturer()";
}

void WebRtcAudioCapturer::AddTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(track);
  DVLOG(1) << "WebRtcAudioCapturer::AddTrack()";

  // Start the source if the first audio track is connected to the capturer.
  // Start() will do nothing if the capturer has already been started.
  Start();

  base::AutoLock auto_lock(lock_);
  // Verify that |track| is not already added to the list.
  DCHECK(std::find_if(tracks_.begin(), tracks_.end(),
                      TrackOwner::TrackWrapper(track)) == tracks_.end());

  if (buffer_.get()) {
    track->SetCaptureFormat(buffer_->params());
  }

  tracks_.push_back(new WebRtcAudioCapturer::TrackOwner(track));
}

void WebRtcAudioCapturer::RemoveTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool stop_source = false;
  {
    base::AutoLock auto_lock(lock_);
    // Get iterator to the first element for which WrapsSink(track) returns
    // true.
    TrackList::iterator it = std::find_if(
        tracks_.begin(), tracks_.end(), TrackOwner::TrackWrapper(track));
    if (it != tracks_.end()) {
      // Clear the delegate to ensure that no more capture callbacks will
      // be sent to this sink. Also avoids a possible crash which can happen
      // if this method is called while capturing is active.
      (*it)->Reset();
      tracks_.erase(it);
    }

    // Stop the source if the last audio track is going away.
    stop_source = tracks_.empty();
  }

  if (stop_source)
    Stop();
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
  bool restart_source = false;
  {
    base::AutoLock auto_lock(lock_);
    if (source_.get() == source.get())
      return;

    source_.swap(old_source);
    source_ = source;
    current_buffer = buffer_;

    // Reset the flag to allow starting the new source.
    restart_source = running_;
    running_ = false;
  }

  const bool no_default_audio_source_exists = !current_buffer.get();

  // Detach the old source from normal recording or perform first-time
  // initialization if Initialize() has never been called. For the second
  // case, the caller is not "taking over an ongoing session" but instead
  // "taking control over a new session".
  if (old_source.get() || no_default_audio_source_exists) {
    DVLOG(1) << "New capture source will now be utilized.";
    if (old_source.get())
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

  if (source.get()) {
    // Make sure to grab the new parameters in case they were reconfigured.
    source->Initialize(current_buffer->params(), this, session_id_);
  }

  if (restart_source)
    Start();
}

void WebRtcAudioCapturer::Start() {
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_)
    return;

  // Start the data source, i.e., start capturing data from the current source.
  // Note that, the source does not have to be a microphone.
  if (source_.get()) {
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

  if (source.get())
    source->Stop();
}

void WebRtcAudioCapturer::SetVolume(int volume) {
  DVLOG(1) << "WebRtcAudioCapturer::SetVolume()";
  DCHECK_LE(volume, MaxVolume());
  double normalized_volume = static_cast<double>(volume) / MaxVolume();
  base::AutoLock auto_lock(lock_);
  if (source_.get())
    source_->SetVolume(normalized_volume);
}

int WebRtcAudioCapturer::Volume() const {
  base::AutoLock auto_lock(lock_);
  return volume_;
}

int WebRtcAudioCapturer::MaxVolume() const {
  return WebRtcAudioDeviceImpl::kMaxVolumeLevel;
}

void WebRtcAudioCapturer::SetAutomaticGainControl(bool enable) {
  base::AutoLock auto_lock(lock_);
  // Store the setting since SetAutomaticGainControl() can be called before
  // Initialize(), in this case stored setting will be applied in Start().
  agc_is_enabled_ = enable;

  if (source_.get())
    source_->SetAutomaticGainControl(enable);
}

void WebRtcAudioCapturer::Capture(media::AudioBus* audio_source,
                                  int audio_delay_milliseconds,
                                  double volume) {
  // This callback is driven by AudioInputDevice::AudioThreadCallback if
  // |source_| is AudioInputDevice, otherwise it is driven by client's
  // CaptureCallback.
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK_LE(volume, 1.0);
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
  // We have a special situation on Linux where the microphone volume can be
  // "higher than maximum". The input volume slider in the sound preference
  // allows the user to set a scaling that is higher than 100%. It means that
  // even if the reported maximum levels is N, the actual microphone level can
  // go up to 1.5x*N and that corresponds to a normalized |volume| of 1.5x.
  DCHECK_LE(volume, 1.6);
#endif

  TrackList tracks;
  scoped_refptr<ConfiguredBuffer> buffer_ref_while_calling;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Map internal volume range of [0.0, 1.0] into [0, 255] used by the
    // webrtc::VoiceEngine. webrtc::VoiceEngine will handle the case when the
    // volume is higher than 255.
    volume_ = static_cast<int>((volume * MaxVolume()) + 0.5);

    // Copy the stuff we will need to local variables. In particular, we grab
    // a reference to the buffer so we can ensure it stays alive even if the
    // buffer is reconfigured while we are calling back.
    buffer_ref_while_calling = buffer_;
    tracks = tracks_;
  }

  int bytes_per_sample =
      buffer_ref_while_calling->params().bits_per_sample() / 8;

  // Interleave, scale, and clip input to int and store result in
  // a local byte buffer.
  audio_source->ToInterleaved(audio_source->frames(), bytes_per_sample,
                              buffer_ref_while_calling->buffer());

  // Feed the data to the tracks.
  for (TrackList::const_iterator it = tracks.begin();
       it != tracks.end();
       ++it) {
    (*it)->CaptureData(buffer_ref_while_calling->buffer(),
                       audio_source->channels(), audio_source->frames(),
                       audio_delay_milliseconds, volume_);
  }
}

void WebRtcAudioCapturer::OnCaptureError() {
  NOTIMPLEMENTED();
}

media::AudioParameters WebRtcAudioCapturer::audio_parameters() const {
  base::AutoLock auto_lock(lock_);
  // |buffer_| can be NULL when SetCapturerSource() or Initialize() has not
  // been called.
  return buffer_.get() ? buffer_->params() : media::AudioParameters();
}

}  // namespace content
