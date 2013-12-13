// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_logging.h"
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

}  // namespace

// Reference counted container of WebRtcLocalAudioTrack delegate.
class WebRtcAudioCapturer::TrackOwner
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer::TrackOwner> {
 public:
  explicit TrackOwner(WebRtcLocalAudioTrack* track)
      : delegate_(track) {}

  void Capture(media::AudioBus* audio_source,
               int audio_delay_milliseconds,
               double volume,
               bool key_pressed) {
    base::AutoLock lock(lock_);
    if (delegate_) {
      delegate_->Capture(audio_source,
                         audio_delay_milliseconds,
                         volume,
                         key_pressed);
    }
  }

  void OnSetFormat(const media::AudioParameters& params) {
    base::AutoLock lock(lock_);
    if (delegate_)
      delegate_->OnSetFormat(params);
  }

  void Reset() {
    base::AutoLock lock(lock_);
    delegate_ = NULL;
  }

  void Stop() {
    base::AutoLock lock(lock_);
    DCHECK(delegate_);

    // This can be reentrant so reset |delegate_| before calling out.
    WebRtcLocalAudioTrack* temp = delegate_;
    delegate_ = NULL;
    temp->Stop();
  }

  // Wrapper which allows to use std::find_if() when adding and removing
  // sinks to/from the list.
  struct TrackWrapper {
    TrackWrapper(WebRtcLocalAudioTrack* track) : track_(track) {}
    bool operator()(
        const scoped_refptr<WebRtcAudioCapturer::TrackOwner>& owner) const {
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

void WebRtcAudioCapturer::Reconfigure(int sample_rate,
    media::ChannelLayout channel_layout,
    int effects) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int buffer_size = GetBufferSize(sample_rate);
  DVLOG(1) << "Using WebRTC input buffer size: " << buffer_size;

  media::AudioParameters::Format format =
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY;

  // bits_per_sample is always 16 for now.
  int bits_per_sample = 16;
  media::AudioParameters params(format, channel_layout, 0, sample_rate,
                                bits_per_sample, buffer_size, effects);
  {
    base::AutoLock auto_lock(lock_);
    params_ = params;

    // Notify all tracks about the new format.
    tracks_.TagAll();
  }
}

bool WebRtcAudioCapturer::Initialize(int render_view_id,
    media::ChannelLayout channel_layout,
    int sample_rate,
    int buffer_size,
    int session_id,
    const std::string& device_id,
    int paired_output_sample_rate,
    int paired_output_frames_per_buffer,
    int effects) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";

  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX);

  WebRtcLogMessage(base::StringPrintf(
      "WAC::Initialize. render_view_id=%d"
      ", channel_layout=%d, sample_rate=%d, buffer_size=%d"
      ", session_id=%d, paired_output_sample_rate=%d"
      ", paired_output_frames_per_buffer=%d",
      render_view_id,
      channel_layout,
      sample_rate,
      buffer_size,
      session_id,
      paired_output_sample_rate,
      paired_output_frames_per_buffer));

  render_view_id_ = render_view_id;
  session_id_ = session_id;
  device_id_ = device_id;
  hardware_buffer_size_ = buffer_size;
  output_sample_rate_ = paired_output_sample_rate;
  output_frames_per_buffer_= paired_output_frames_per_buffer;

  if (render_view_id == -1) {
    // Return true here to allow injecting a new source via SetCapturerSource()
    // at a later state.
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

  // Create and configure the default audio capturing source. The |source_|
  // will be overwritten if an external client later calls SetCapturerSource()
  // providing an alternative media::AudioCapturerSource.
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(render_view_id),
                    channel_layout,
                    static_cast<float>(sample_rate),
                    effects);

  return true;
}

WebRtcAudioCapturer::WebRtcAudioCapturer()
    : running_(false),
      render_view_id_(-1),
      hardware_buffer_size_(0),
      session_id_(0),
      volume_(0),
      peer_connection_mode_(false),
      output_sample_rate_(0),
      output_frames_per_buffer_(0),
      key_pressed_(false) {
  DVLOG(1) << "WebRtcAudioCapturer::WebRtcAudioCapturer()";
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(tracks_.IsEmpty());
  DCHECK(!running_);
  DVLOG(1) << "WebRtcAudioCapturer::~WebRtcAudioCapturer()";
}

void WebRtcAudioCapturer::AddTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(track);
  DVLOG(1) << "WebRtcAudioCapturer::AddTrack()";

  {
    base::AutoLock auto_lock(lock_);
    // Verify that |track| is not already added to the list.
    DCHECK(!tracks_.Contains(TrackOwner::TrackWrapper(track)));

    // Add with a tag, so we remember to call OnSetFormat() on the new
    // track.
    scoped_refptr<TrackOwner> track_owner(new TrackOwner(track));
    tracks_.AddAndTag(track_owner);
  }

  // Start the source if the first audio track is connected to the capturer.
  // Start() will do nothing if the capturer has already been started.
  Start();

}

void WebRtcAudioCapturer::RemoveTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool stop_source = false;
  {
    base::AutoLock auto_lock(lock_);

    scoped_refptr<TrackOwner> removed_item =
        tracks_.Remove(TrackOwner::TrackWrapper(track));

    // Clear the delegate to ensure that no more capture callbacks will
    // be sent to this sink. Also avoids a possible crash which can happen
    // if this method is called while capturing is active.
    if (removed_item.get())
      removed_item->Reset();

    // Stop the source if the last audio track is going away.
    stop_source = tracks_.IsEmpty();
  }

  if (stop_source)
    Stop();
}

void WebRtcAudioCapturer::SetCapturerSource(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::ChannelLayout channel_layout,
    float sample_rate,
    int effects) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "SetCapturerSource(channel_layout=" << channel_layout << ","
           << "sample_rate=" << sample_rate << ")";
  scoped_refptr<media::AudioCapturerSource> old_source;
  bool restart_source = false;
  {
    base::AutoLock auto_lock(lock_);
    if (source_.get() == source.get())
      return;

    source_.swap(old_source);
    source_ = source;

    // Reset the flag to allow starting the new source.
    restart_source = running_;
    running_ = false;
  }

  DVLOG(1) << "Switching to a new capture source.";
  if (old_source.get())
    old_source->Stop();

  // Dispatch the new parameters both to the sink(s) and to the new source.
  // The idea is to get rid of any dependency of the microphone parameters
  // which would normally be used by default.
  Reconfigure(sample_rate, channel_layout, effects);

  // Make sure to grab the new parameters in case they were reconfigured.
  media::AudioParameters params = audio_parameters();
  if (source.get())
    source->Initialize(params, this, session_id_);

  if (restart_source)
    Start();
}

void WebRtcAudioCapturer::EnablePeerConnectionMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "EnablePeerConnectionMode";
  // Do nothing if the peer connection mode has been enabled.
  if (peer_connection_mode_)
    return;

  peer_connection_mode_ = true;
  int render_view_id = -1;
  {
    base::AutoLock auto_lock(lock_);
    // Simply return if there is no existing source or the |render_view_id_| is
    // not valid.
    if (!source_.get() || render_view_id_== -1)
      return;

    render_view_id = render_view_id_;
  }

  // Do nothing if the current buffer size is the WebRtc native buffer size.
  media::AudioParameters params = audio_parameters();
  if (GetBufferSize(params.sample_rate()) == params.frames_per_buffer())
    return;

  // Create a new audio stream as source which will open the hardware using
  // WebRtc native buffer size.
  SetCapturerSource(AudioDeviceFactory::NewInputDevice(render_view_id),
                    params.channel_layout(),
                    static_cast<float>(params.sample_rate()),
                    params.effects());
}

void WebRtcAudioCapturer::Start() {
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_ || !source_)
    return;

  // Start the data source, i.e., start capturing data from the current source.
  // We need to set the AGC control before starting the stream.
  source_->SetAutomaticGainControl(true);
  source_->Start();
  running_ = true;
}

void WebRtcAudioCapturer::Stop() {
  DVLOG(1) << "WebRtcAudioCapturer::Stop()";
  scoped_refptr<media::AudioCapturerSource> source;
  TrackList::ItemList tracks;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    source = source_;
    tracks = tracks_.Items();
    tracks_.Clear();
    running_ = false;
  }

  for (TrackList::ItemList::const_iterator it = tracks.begin();
       it != tracks.end();
       ++it) {
    (*it)->Stop();
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

void WebRtcAudioCapturer::Capture(media::AudioBus* audio_source,
                                  int audio_delay_milliseconds,
                                  double volume,
                                  bool key_pressed) {
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

  TrackList::ItemList tracks;
  TrackList::ItemList tracks_to_notify_format;
  int current_volume = 0;
  media::AudioParameters params;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Map internal volume range of [0.0, 1.0] into [0, 255] used by the
    // webrtc::VoiceEngine. webrtc::VoiceEngine will handle the case when the
    // volume is higher than 255.
    volume_ = static_cast<int>((volume * MaxVolume()) + 0.5);
    current_volume = volume_;
    audio_delay_ = base::TimeDelta::FromMilliseconds(audio_delay_milliseconds);
    key_pressed_ = key_pressed;
    tracks = tracks_.Items();
    tracks_.RetrieveAndClearTags(&tracks_to_notify_format);

    CHECK(params_.IsValid());
    CHECK_EQ(audio_source->channels(), params_.channels());
    CHECK_EQ(audio_source->frames(), params_.frames_per_buffer());
    params = params_;
  }

  // Notify the tracks on when the format changes. This will do nothing if
  // |tracks_to_notify_format| is empty.
  for (TrackList::ItemList::const_iterator it = tracks_to_notify_format.begin();
       it != tracks_to_notify_format.end(); ++it) {
    (*it)->OnSetFormat(params);
  }

  // Feed the data to the tracks.
  for (TrackList::ItemList::const_iterator it = tracks.begin();
       it != tracks.end();
       ++it) {
    (*it)->Capture(audio_source, audio_delay_milliseconds,
                   current_volume, key_pressed);
  }
}

void WebRtcAudioCapturer::OnCaptureError() {
  NOTIMPLEMENTED();
}

media::AudioParameters WebRtcAudioCapturer::audio_parameters() const {
  base::AutoLock auto_lock(lock_);
  return params_;
}

bool WebRtcAudioCapturer::GetPairedOutputParameters(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) const {
  // Don't set output parameters unless all of them are valid.
  if (session_id_ <= 0 || !output_sample_rate_ || !output_frames_per_buffer_)
    return false;

  *session_id = session_id_;
  *output_sample_rate = output_sample_rate_;
  *output_frames_per_buffer = output_frames_per_buffer_;

  return true;
}

int WebRtcAudioCapturer::GetBufferSize(int sample_rate) const {
  DCHECK(thread_checker_.CalledOnValidThread());
#if defined(OS_ANDROID)
  // TODO(henrika): Tune and adjust buffer size on Android.
  return (2 * sample_rate / 100);
#endif

  // PeerConnection is running at a buffer size of 10ms data. A multiple of
  // 10ms as the buffer size can give the best performance to PeerConnection.
  int peer_connection_buffer_size = sample_rate / 100;

  // Use the native hardware buffer size in non peer connection mode when the
  // platform is using a native buffer size smaller than the PeerConnection
  // buffer size.
  if (!peer_connection_mode_ && hardware_buffer_size_ &&
      hardware_buffer_size_ <= peer_connection_buffer_size) {
    return hardware_buffer_size_;
  }

  return (sample_rate / 100);
}

void WebRtcAudioCapturer::GetAudioProcessingParams(
    base::TimeDelta* delay, int* volume, bool* key_pressed) {
  base::AutoLock auto_lock(lock_);
  *delay = audio_delay_;
  *volume = volume_;
  *key_pressed = key_pressed_;
}

}  // namespace content
