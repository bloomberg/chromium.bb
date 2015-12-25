// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_capturer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_processor_options.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/media/webrtc_logging.h"
#include "media/audio/sample_rates.h"

namespace content {

namespace {

// Audio buffer sizes are specified in milliseconds.
const char kAudioLatency[] = "latencyMs";
const int kMinAudioLatencyMs = 0;
const int kMaxAudioLatencyMs = 10000;

// Method to check if any of the data in |audio_source| has energy.
bool HasDataEnergy(const media::AudioBus& audio_source) {
  for (int ch = 0; ch < audio_source.channels(); ++ch) {
    const float* channel_ptr = audio_source.channel(ch);
    for (int frame = 0; frame < audio_source.frames(); ++frame) {
      if (channel_ptr[frame] != 0)
        return true;
    }
  }

  // All the data is zero.
  return false;
}

}  // namespace

// Reference counted container of WebRtcLocalAudioTrack delegate.
// TODO(xians): Switch to MediaStreamAudioSinkOwner.
class WebRtcAudioCapturer::TrackOwner
    : public base::RefCountedThreadSafe<WebRtcAudioCapturer::TrackOwner> {
 public:
  explicit TrackOwner(WebRtcLocalAudioTrack* track)
      : delegate_(track) {}

  void Capture(const media::AudioBus& audio_bus,
               base::TimeTicks estimated_capture_time,
               bool force_report_nonzero_energy) {
    base::AutoLock lock(lock_);
    if (delegate_) {
      delegate_->Capture(audio_bus,
                         estimated_capture_time,
                         force_report_nonzero_energy);
    }
  }

  void OnSetFormat(const media::AudioParameters& params) {
    base::AutoLock lock(lock_);
    if (delegate_)
      delegate_->OnSetFormat(params);
  }

  void SetAudioProcessor(
      const scoped_refptr<MediaStreamAudioProcessor>& processor) {
    base::AutoLock lock(lock_);
    if (delegate_)
      delegate_->SetAudioProcessor(processor);
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
    explicit TrackWrapper(WebRtcLocalAudioTrack* track) : track_(track) {}
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
scoped_refptr<WebRtcAudioCapturer> WebRtcAudioCapturer::CreateCapturer(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    WebRtcAudioDeviceImpl* audio_device,
    MediaStreamAudioSource* audio_source) {
  scoped_refptr<WebRtcAudioCapturer> capturer = new WebRtcAudioCapturer(
      render_frame_id, device_info, constraints, audio_device, audio_source);
  if (capturer->Initialize())
    return capturer;

  return NULL;
}

bool WebRtcAudioCapturer::Initialize() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Initialize()";
  WebRtcLogMessage(base::StringPrintf(
      "WAC::Initialize. render_frame_id=%d"
      ", channel_layout=%d, sample_rate=%d, buffer_size=%d"
      ", session_id=%d, paired_output_sample_rate=%d"
      ", paired_output_frames_per_buffer=%d, effects=%d. ",
      render_frame_id_, device_info_.device.input.channel_layout,
      device_info_.device.input.sample_rate,
      device_info_.device.input.frames_per_buffer, device_info_.session_id,
      device_info_.device.matched_output.sample_rate,
      device_info_.device.matched_output.frames_per_buffer,
      device_info_.device.input.effects));

  if (render_frame_id_ == -1) {
    // Return true here to allow injecting a new source via
    // SetCapturerSourceForTesting() at a later state.
    return true;
  }

  MediaAudioConstraints audio_constraints(constraints_,
                                          device_info_.device.input.effects);
  if (!audio_constraints.IsValid())
    return false;

  media::ChannelLayout channel_layout = static_cast<media::ChannelLayout>(
      device_info_.device.input.channel_layout);

  // If KEYBOARD_MIC effect is set, change the layout to the corresponding
  // layout that includes the keyboard mic.
  if ((device_info_.device.input.effects &
          media::AudioParameters::KEYBOARD_MIC) &&
      audio_constraints.GetProperty(
          MediaAudioConstraints::kGoogExperimentalNoiseSuppression)) {
    if (channel_layout == media::CHANNEL_LAYOUT_STEREO) {
      channel_layout = media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
      DVLOG(1) << "Changed stereo layout to stereo + keyboard mic layout due "
               << "to KEYBOARD_MIC effect.";
    } else {
      DVLOG(1) << "KEYBOARD_MIC effect ignored, not compatible with layout "
               << channel_layout;
    }
  }

  DVLOG(1) << "Audio input hardware channel layout: " << channel_layout;
  UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioInputChannelLayout",
                            channel_layout, media::CHANNEL_LAYOUT_MAX + 1);

  // Verify that the reported input channel configuration is supported.
  if (channel_layout != media::CHANNEL_LAYOUT_MONO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO &&
      channel_layout != media::CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC) {
    DLOG(ERROR) << channel_layout
                << " is not a supported input channel configuration.";
    return false;
  }

  DVLOG(1) << "Audio input hardware sample rate: "
           << device_info_.device.input.sample_rate;
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(device_info_.device.input.sample_rate, &asr)) {
    UMA_HISTOGRAM_ENUMERATION(
        "WebRTC.AudioInputSampleRate", asr, media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioInputSampleRateUnexpected",
                         device_info_.device.input.sample_rate);
  }

  // Initialize the buffer size to zero, which means it wasn't specified.
  // If it is out of range, we return it to zero.
  int buffer_size_ms = 0;
  int buffer_size_samples = 0;
  GetConstraintValueAsInteger(constraints_, kAudioLatency, &buffer_size_ms);
  if (buffer_size_ms < kMinAudioLatencyMs ||
      buffer_size_ms > kMaxAudioLatencyMs) {
    DVLOG(1) << "Ignoring out of range buffer size " << buffer_size_ms;
  } else {
    buffer_size_samples =
        device_info_.device.input.sample_rate * buffer_size_ms / 1000;
  }
  DVLOG_IF(1, buffer_size_samples > 0)
      << "Custom audio buffer size: " << buffer_size_samples << " samples";

  // Create and configure the default audio capturing source.
  SetCapturerSourceInternal(
      AudioDeviceFactory::NewInputDevice(render_frame_id_),
      channel_layout,
      device_info_.device.input.sample_rate,
      buffer_size_samples);

  // Add the capturer to the WebRtcAudioDeviceImpl since it needs some hardware
  // information from the capturer.
  if (audio_device_)
    audio_device_->AddAudioCapturer(this);

  return true;
}

WebRtcAudioCapturer::WebRtcAudioCapturer(
    int render_frame_id,
    const StreamDeviceInfo& device_info,
    const blink::WebMediaConstraints& constraints,
    WebRtcAudioDeviceImpl* audio_device,
    MediaStreamAudioSource* audio_source)
    : constraints_(constraints),
      audio_processor_(new rtc::RefCountedObject<MediaStreamAudioProcessor>(
          constraints,
          device_info.device.input,
          audio_device)),
      running_(false),
      render_frame_id_(render_frame_id),
      device_info_(device_info),
      volume_(0),
      peer_connection_mode_(false),
      audio_device_(audio_device),
      audio_source_(audio_source) {
  DVLOG(1) << "WebRtcAudioCapturer::WebRtcAudioCapturer()";
}

WebRtcAudioCapturer::~WebRtcAudioCapturer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(tracks_.IsEmpty());
  DVLOG(1) << "WebRtcAudioCapturer::~WebRtcAudioCapturer()";
  Stop();
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
    tracks_.AddAndTag(track_owner.get());
  }
}

void WebRtcAudioCapturer::RemoveTrack(WebRtcLocalAudioTrack* track) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::RemoveTrack()";
  bool stop_source = false;
  {
    base::AutoLock auto_lock(lock_);

    scoped_refptr<TrackOwner> removed_item =
        tracks_.Remove(TrackOwner::TrackWrapper(track));

    // Clear the delegate to ensure that no more capture callbacks will
    // be sent to this sink. Also avoids a possible crash which can happen
    // if this method is called while capturing is active.
    if (removed_item.get()) {
      removed_item->Reset();
      stop_source = tracks_.IsEmpty();
    }
  }
  if (stop_source) {
    // Since WebRtcAudioCapturer does not inherit MediaStreamAudioSource,
    // and instead MediaStreamAudioSource is composed of a WebRtcAudioCapturer,
    // we have to call StopSource on the MediaStreamSource. This will call
    // MediaStreamAudioSource::DoStopSource which in turn call
    // WebRtcAudioCapturerer::Stop();
    audio_source_->StopSource();
  }
}

void WebRtcAudioCapturer::SetCapturerSourceInternal(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::ChannelLayout channel_layout,
    int sample_rate,
    int buffer_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "SetCapturerSource(channel_layout=" << channel_layout << ","
           << "sample_rate=" << sample_rate << ")";
  scoped_refptr<media::AudioCapturerSource> old_source;
  {
    base::AutoLock auto_lock(lock_);
    if (source_.get() == source.get())
      return;

    source_.swap(old_source);
    source_ = source;

    // Reset the flag to allow starting the new source.
    running_ = false;
  }

  DVLOG(1) << "Switching to a new capture source.";
  if (old_source.get())
    old_source->Stop();

  // If the buffer size is zero, it has not been specified.
  // We either default to 10ms, or use the hardware buffer size.
  if (buffer_size == 0)
    buffer_size = GetBufferSize(sample_rate);

  // Dispatch the new parameters both to the sink(s) and to the new source,
  // also apply the new |constraints|.
  // The idea is to get rid of any dependency of the microphone parameters
  // which would normally be used by default.
  // bits_per_sample is always 16 for now.
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                channel_layout, sample_rate, 16, buffer_size);
  params.set_effects(device_info_.device.input.effects);

  {
    base::AutoLock auto_lock(lock_);
    // Notify the |audio_processor_| of the new format.
    audio_processor_->OnCaptureFormatChanged(params);

    // Notify all tracks about the new format.
    tracks_.TagAll();
  }

  if (source.get())
    source->Initialize(params, this, session_id());

  Start();
}

void WebRtcAudioCapturer::EnablePeerConnectionMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "EnablePeerConnectionMode";
  // Do nothing if the peer connection mode has been enabled.
  if (peer_connection_mode_)
    return;

  peer_connection_mode_ = true;
  int render_frame_id = -1;
  media::AudioParameters input_params;
  {
    base::AutoLock auto_lock(lock_);
    // Simply return if there is no existing source or the |render_frame_id_| is
    // not valid.
    if (!source_.get() || render_frame_id_ == -1)
      return;

    render_frame_id = render_frame_id_;
    input_params = audio_processor_->InputFormat();
  }

  // Do nothing if the current buffer size is the WebRtc native buffer size.
  if (GetBufferSize(input_params.sample_rate()) ==
          input_params.frames_per_buffer()) {
    return;
  }

  // Create a new audio stream as source which will open the hardware using
  // WebRtc native buffer size.
  SetCapturerSourceInternal(AudioDeviceFactory::NewInputDevice(render_frame_id),
                            input_params.channel_layout(),
                            input_params.sample_rate(),
                            0);
}

void WebRtcAudioCapturer::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcAudioCapturer::Start()";
  base::AutoLock auto_lock(lock_);
  if (running_ || !source_.get())
    return;

  // Start the data source, i.e., start capturing data from the current source.
  // We need to set the AGC control before starting the stream.
  source_->SetAutomaticGainControl(true);
  source_->Start();
  running_ = true;
}

void WebRtcAudioCapturer::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
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

  // Remove the capturer object from the WebRtcAudioDeviceImpl.
  if (audio_device_)
    audio_device_->RemoveAudioCapturer(this);

  for (TrackList::ItemList::const_iterator it = tracks.begin();
       it != tracks.end();
       ++it) {
    (*it)->Stop();
  }

  if (source.get())
    source->Stop();

  // Stop the audio processor to avoid feeding render data into the processor.
  audio_processor_->Stop();
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

media::AudioParameters WebRtcAudioCapturer::GetOutputFormat() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return audio_processor_->OutputFormat();
}

void WebRtcAudioCapturer::Capture(const media::AudioBus* audio_source,
                                  int audio_delay_milliseconds,
                                  double volume,
                                  bool key_pressed) {
// This callback is driven by AudioInputDevice::AudioThreadCallback if
// |source_| is AudioInputDevice, otherwise it is driven by client's
// CaptureCallback.
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK_LE(volume, 1.0);
#elif (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_OPENBSD)
  // We have a special situation on Linux where the microphone volume can be
  // "higher than maximum". The input volume slider in the sound preference
  // allows the user to set a scaling that is higher than 100%. It means that
  // even if the reported maximum levels is N, the actual microphone level can
  // go up to 1.5x*N and that corresponds to a normalized |volume| of 1.5x.
  DCHECK_LE(volume, 1.6);
#endif

  // TODO(miu): Plumbing is needed to determine the actual capture timestamp
  // of the audio, instead of just snapshotting TimeTicks::Now(), for proper
  // audio/video sync.  http://crbug.com/335335
  const base::TimeTicks reference_clock_snapshot = base::TimeTicks::Now();

  TrackList::ItemList tracks;
  TrackList::ItemList tracks_to_notify_format;
  int current_volume = 0;
  {
    base::AutoLock auto_lock(lock_);
    if (!running_)
      return;

    // Map internal volume range of [0.0, 1.0] into [0, 255] used by AGC.
    // The volume can be higher than 255 on Linux, and it will be cropped to
    // 255 since AGC does not allow values out of range.
    volume_ = static_cast<int>((volume * MaxVolume()) + 0.5);
    current_volume = volume_ > MaxVolume() ? MaxVolume() : volume_;
    tracks = tracks_.Items();
    tracks_.RetrieveAndClearTags(&tracks_to_notify_format);
  }

  DCHECK(audio_processor_->InputFormat().IsValid());
  DCHECK_EQ(audio_source->channels(),
            audio_processor_->InputFormat().channels());
  DCHECK_EQ(audio_source->frames(),
            audio_processor_->InputFormat().frames_per_buffer());

  // Notify the tracks on when the format changes. This will do nothing if
  // |tracks_to_notify_format| is empty.
  const media::AudioParameters& output_params =
      audio_processor_->OutputFormat();
  for (const auto& track : tracks_to_notify_format) {
    track->OnSetFormat(output_params);
    track->SetAudioProcessor(audio_processor_);
  }

  // Figure out if the pre-processed data has any energy or not, the
  // information will be passed to the track to force the calculator
  // to report energy in case the post-processed data is zeroed by the audio
  // processing.
  const bool force_report_nonzero_energy = HasDataEnergy(*audio_source);

  // Push the data to the processor for processing.
  audio_processor_->PushCaptureData(
      *audio_source,
      base::TimeDelta::FromMilliseconds(audio_delay_milliseconds));

  // Process and consume the data in the processor until there is not enough
  // data in the processor.
  media::AudioBus* processed_data = nullptr;
  base::TimeDelta processed_data_audio_delay;
  int new_volume = 0;
  while (audio_processor_->ProcessAndConsumeData(
             current_volume, key_pressed,
             &processed_data, &processed_data_audio_delay, &new_volume)) {
    DCHECK(processed_data);
    const base::TimeTicks processed_data_capture_time =
        reference_clock_snapshot - processed_data_audio_delay;
    for (const auto& track : tracks) {
      track->Capture(*processed_data,
                     processed_data_capture_time,
                     force_report_nonzero_energy);
    }

    if (new_volume) {
      SetVolume(new_volume);

      // Update the |current_volume| to avoid passing the old volume to AGC.
      current_volume = new_volume;
    }
  }
}

void WebRtcAudioCapturer::OnCaptureError(const std::string& message) {
  WebRtcLogMessage("WAC::OnCaptureError: " + message);
}

media::AudioParameters WebRtcAudioCapturer::source_audio_parameters() const {
  base::AutoLock auto_lock(lock_);
  return audio_processor_.get() ? audio_processor_->InputFormat()
                                : media::AudioParameters();
}

bool WebRtcAudioCapturer::GetPairedOutputParameters(
    int* session_id,
    int* output_sample_rate,
    int* output_frames_per_buffer) const {
  // Don't set output parameters unless all of them are valid.
  if (device_info_.session_id <= 0 ||
      !device_info_.device.matched_output.sample_rate ||
      !device_info_.device.matched_output.frames_per_buffer)
    return false;

  *session_id = device_info_.session_id;
  *output_sample_rate = device_info_.device.matched_output.sample_rate;
  *output_frames_per_buffer =
      device_info_.device.matched_output.frames_per_buffer;

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
  // buffer size and audio processing is off.
  int hardware_buffer_size = device_info_.device.input.frames_per_buffer;
  if (!peer_connection_mode_ && hardware_buffer_size &&
      hardware_buffer_size <= peer_connection_buffer_size &&
      !audio_processor_->has_audio_processing()) {
    DVLOG(1) << "WebRtcAudioCapturer is using hardware buffer size "
             << hardware_buffer_size;
    return hardware_buffer_size;
  }

  return (sample_rate / 100);
}

void WebRtcAudioCapturer::SetCapturerSource(
    const scoped_refptr<media::AudioCapturerSource>& source,
    media::AudioParameters params) {
  // Create a new audio stream as source which uses the new source.
  SetCapturerSourceInternal(source,
                            params.channel_layout(),
                            params.sample_rate(),
                            0);
}

}  // namespace content
