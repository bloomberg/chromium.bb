// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_renderer.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/webrtc/peer_connection_remote_audio_source.h"
#include "content/renderer/media/webrtc_logging.h"
#include "media/audio/sample_rates.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "media/audio/win/core_audio_util_win.h"
#endif

namespace content {

namespace {

// Audio parameters that don't change.
const media::AudioParameters::Format kFormat =
    media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
const media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
const int kChannels = 2;
const int kBitsPerSample = 16;

// Used for UMA histograms.
const int kRenderTimeHistogramMinMicroseconds = 100;
const int kRenderTimeHistogramMaxMicroseconds = 1 * 1000 * 1000;  // 1 second

// This is a simple wrapper class that's handed out to users of a shared
// WebRtcAudioRenderer instance.  This class maintains the per-user 'playing'
// and 'started' states to avoid problems related to incorrect usage which
// might violate the implementation assumptions inside WebRtcAudioRenderer
// (see the play reference count).
class SharedAudioRenderer : public MediaStreamAudioRenderer {
 public:
  // Callback definition for a callback that is called when when Play(), Pause()
  // or SetVolume are called (whenever the internal |playing_state_| changes).
  using OnPlayStateChanged =
      base::Callback<void(const blink::WebMediaStream&,
                          WebRtcAudioRenderer::PlayingState*)>;

  // Signals that the PlayingState* is about to become invalid, see comment in
  // OnPlayStateRemoved.
  using OnPlayStateRemoved =
      base::OnceCallback<void(WebRtcAudioRenderer::PlayingState*)>;

  SharedAudioRenderer(const scoped_refptr<MediaStreamAudioRenderer>& delegate,
                      const blink::WebMediaStream& media_stream,
                      const OnPlayStateChanged& on_play_state_changed,
                      OnPlayStateRemoved on_play_state_removed)
      : delegate_(delegate),
        media_stream_(media_stream),
        started_(false),
        on_play_state_changed_(on_play_state_changed),
        on_play_state_removed_(std::move(on_play_state_removed)) {
    DCHECK(!on_play_state_changed_.is_null());
    DCHECK(!media_stream_.IsNull());
  }

 protected:
  ~SharedAudioRenderer() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DVLOG(1) << __func__;
    Stop();
    std::move(on_play_state_removed_).Run(&playing_state_);
  }

  void Start() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (started_)
      return;
    started_ = true;
    delegate_->Start();
  }

  void Play() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(started_);
    if (playing_state_.playing())
      return;
    playing_state_.set_playing(true);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void Pause() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(started_);
    if (!playing_state_.playing())
      return;
    playing_state_.set_playing(false);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  void Stop() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!started_)
      return;
    Pause();
    started_ = false;
    delegate_->Stop();
  }

  void SetVolume(float volume) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(volume >= 0.0f && volume <= 1.0f);
    playing_state_.set_volume(volume);
    on_play_state_changed_.Run(media_stream_, &playing_state_);
  }

  media::OutputDeviceInfo GetOutputDeviceInfo() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return delegate_->GetOutputDeviceInfo();
  }

  void SwitchOutputDevice(
      const std::string& device_id,
      const url::Origin& security_origin,
      const media::OutputDeviceStatusCB& callback) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return delegate_->SwitchOutputDevice(device_id, security_origin, callback);
  }

  base::TimeDelta GetCurrentRenderTime() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return delegate_->GetCurrentRenderTime();
  }

  bool IsLocalRenderer() const override {
    DCHECK(thread_checker_.CalledOnValidThread());
    return delegate_->IsLocalRenderer();
  }

 private:
  base::ThreadChecker thread_checker_;
  const scoped_refptr<MediaStreamAudioRenderer> delegate_;
  const blink::WebMediaStream media_stream_;
  bool started_;
  WebRtcAudioRenderer::PlayingState playing_state_;
  OnPlayStateChanged on_play_state_changed_;
  OnPlayStateRemoved on_play_state_removed_;
};

}  // namespace

WebRtcAudioRenderer::WebRtcAudioRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread,
    const blink::WebMediaStream& media_stream,
    int source_render_frame_id,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin)
    : state_(UNINITIALIZED),
      source_render_frame_id_(source_render_frame_id),
      session_id_(session_id),
      signaling_thread_(signaling_thread),
      media_stream_(media_stream),
      source_(NULL),
      play_ref_count_(0),
      start_ref_count_(0),
      sink_params_(kFormat, kChannelLayout, 0, kBitsPerSample, 0),
      output_device_id_(device_id),
      security_origin_(security_origin) {
  WebRtcLogMessage(base::StringPrintf(
      "WAR::WAR. source_render_frame_id=%d, session_id=%d, effects=%i",
      source_render_frame_id, session_id, sink_params_.effects()));
}

WebRtcAudioRenderer::~WebRtcAudioRenderer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, UNINITIALIZED);
}

bool WebRtcAudioRenderer::Initialize(WebRtcAudioRendererSource* source) {
  DVLOG(1) << "WebRtcAudioRenderer::Initialize()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(source);
  DCHECK(!sink_.get());
  DCHECK_GE(session_id_, 0);
  {
    base::AutoLock auto_lock(lock_);
    DCHECK_EQ(state_, UNINITIALIZED);
    DCHECK(!source_);
  }

  sink_ = AudioDeviceFactory::NewAudioRendererSink(
      AudioDeviceFactory::kSourceWebRtc, source_render_frame_id_, session_id_,
      output_device_id_, security_origin_);

  if (sink_->GetOutputDeviceInfo().device_status() !=
      media::OUTPUT_DEVICE_STATUS_OK) {
    return false;
  }

  PrepareSink();
  {
    // No need to reassert the preconditions because the other thread accessing
    // the fields only reads them.
    base::AutoLock auto_lock(lock_);
    source_ = source;

    // User must call Play() before any audio can be heard.
    state_ = PAUSED;
  }
  sink_->Start();
  sink_->Play();  // Not all the sinks play on start.

  return true;
}

scoped_refptr<MediaStreamAudioRenderer>
WebRtcAudioRenderer::CreateSharedAudioRendererProxy(
    const blink::WebMediaStream& media_stream) {
  SharedAudioRenderer::OnPlayStateChanged on_play_state_changed =
      base::Bind(&WebRtcAudioRenderer::OnPlayStateChanged, this);
  SharedAudioRenderer::OnPlayStateRemoved on_play_state_removed =
      base::BindOnce(&WebRtcAudioRenderer::OnPlayStateRemoved, this);
  return new SharedAudioRenderer(this, media_stream, on_play_state_changed,
                                 std::move(on_play_state_removed));
}

bool WebRtcAudioRenderer::IsStarted() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return start_ref_count_ != 0;
}

bool WebRtcAudioRenderer::CurrentThreadIsRenderingThread() {
  return sink_->CurrentThreadIsRenderingThread();
}

void WebRtcAudioRenderer::Start() {
  DVLOG(1) << "WebRtcAudioRenderer::Start()";
  DCHECK(thread_checker_.CalledOnValidThread());
  ++start_ref_count_;
}

void WebRtcAudioRenderer::Play() {
  DVLOG(1) << "WebRtcAudioRenderer::Play()";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (playing_state_.playing())
    return;

  playing_state_.set_playing(true);

  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPlayState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPlayState()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK(play_ref_count_ == 0 || state_ == PLAYING);
  ++play_ref_count_;

  if (state_ != PLAYING) {
    state_ = PLAYING;

    if (audio_fifo_) {
      audio_delay_ = base::TimeDelta();
      audio_fifo_->Clear();
    }
  }
}

void WebRtcAudioRenderer::Pause() {
  DVLOG(1) << "WebRtcAudioRenderer::Pause()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!playing_state_.playing())
    return;

  playing_state_.set_playing(false);

  OnPlayStateChanged(media_stream_, &playing_state_);
}

void WebRtcAudioRenderer::EnterPauseState() {
  DVLOG(1) << "WebRtcAudioRenderer::EnterPauseState()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(start_ref_count_, 0) << "Did you forget to call Start()?";
  base::AutoLock auto_lock(lock_);
  if (state_ == UNINITIALIZED)
    return;

  DCHECK_EQ(state_, PLAYING);
  DCHECK_GT(play_ref_count_, 0);
  if (!--play_ref_count_)
    state_ = PAUSED;
}

void WebRtcAudioRenderer::Stop() {
  DVLOG(1) << "WebRtcAudioRenderer::Stop()";
  DCHECK(thread_checker_.CalledOnValidThread());
  {
    base::AutoLock auto_lock(lock_);
    if (state_ == UNINITIALIZED)
      return;

    if (--start_ref_count_)
      return;

    DVLOG(1) << "Calling RemoveAudioRenderer and Stop().";

    source_->RemoveAudioRenderer(this);
    source_ = NULL;
    state_ = UNINITIALIZED;
  }

  // Apart from here, |max_render_time_| is only accessed in SourceCallback(),
  // which is guaranteed to not run after |source_| has been set to null, and
  // not before this function has returned.
  // If |max_render_time_| is zero, no render call has been made.
  if (!max_render_time_.is_zero()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Media.Audio.Render.GetSourceDataTimeMax.WebRTC",
        max_render_time_.InMicroseconds(),
        kRenderTimeHistogramMinMicroseconds,
        kRenderTimeHistogramMaxMicroseconds, 50);
    max_render_time_ = base::TimeDelta();
  }

  // Make sure to stop the sink while _not_ holding the lock since the Render()
  // callback may currently be executing and trying to grab the lock while we're
  // stopping the thread on which it runs.
  sink_->Stop();
}

void WebRtcAudioRenderer::SetVolume(float volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(volume >= 0.0f && volume <= 1.0f);

  playing_state_.set_volume(volume);
  OnPlayStateChanged(media_stream_, &playing_state_);
}

media::OutputDeviceInfo WebRtcAudioRenderer::GetOutputDeviceInfo() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return sink_ ? sink_->GetOutputDeviceInfo() : media::OutputDeviceInfo();
}

base::TimeDelta WebRtcAudioRenderer::GetCurrentRenderTime() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return current_time_;
}

bool WebRtcAudioRenderer::IsLocalRenderer() const {
  return false;
}

void WebRtcAudioRenderer::SwitchOutputDevice(
    const std::string& device_id,
    const url::Origin& security_origin,
    const media::OutputDeviceStatusCB& callback) {
  DVLOG(1) << "WebRtcAudioRenderer::SwitchOutputDevice()";
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GE(session_id_, 0);
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(source_);
    DCHECK_NE(state_, UNINITIALIZED);
  }

  scoped_refptr<media::AudioRendererSink> new_sink =
      AudioDeviceFactory::NewAudioRendererSink(
          AudioDeviceFactory::kSourceWebRtc, source_render_frame_id_,
          session_id_, device_id, security_origin);
  media::OutputDeviceStatus status =
      new_sink->GetOutputDeviceInfo().device_status();
  if (status != media::OUTPUT_DEVICE_STATUS_OK) {
    callback.Run(status);
    return;
  }

  // Make sure to stop the sink while _not_ holding the lock since the Render()
  // callback may currently be executing and trying to grab the lock while we're
  // stopping the thread on which it runs.
  sink_->Stop();
  sink_ = new_sink;
  output_device_id_ = device_id;
  security_origin_ = security_origin;
  {
    base::AutoLock auto_lock(lock_);
    source_->AudioRendererThreadStopped();
  }
  PrepareSink();
  sink_->Start();
  sink_->Play();  // Not all the sinks play on start.

  callback.Run(media::OUTPUT_DEVICE_STATUS_OK);
}

int WebRtcAudioRenderer::Render(base::TimeDelta delay,
                                base::TimeTicks delay_timestamp,
                                int prior_frames_skipped,
                                media::AudioBus* audio_bus) {
  DCHECK(sink_->CurrentThreadIsRenderingThread());
  base::AutoLock auto_lock(lock_);
  if (!source_)
    return 0;

  DVLOG(2) << "WebRtcAudioRenderer::Render()";
  DVLOG(2) << "audio_delay: " << delay;

  audio_delay_ = delay;

  // If there are skipped frames, pull and throw away the same amount. We always
  // pull 10 ms of data from the source (see PrepareSink()), so the fifo is only
  // required if the number of frames to drop doesn't correspond to 10 ms.
  if (prior_frames_skipped > 0) {
    const int source_frames_per_buffer = sink_params_.sample_rate() / 100;
    if (!audio_fifo_ && prior_frames_skipped != source_frames_per_buffer) {
      audio_fifo_.reset(new media::AudioPullFifo(
          kChannels, source_frames_per_buffer,
          base::Bind(&WebRtcAudioRenderer::SourceCallback,
                     base::Unretained(this))));
    }

    std::unique_ptr<media::AudioBus> drop_bus =
        media::AudioBus::Create(audio_bus->channels(), prior_frames_skipped);
    if (audio_fifo_)
      audio_fifo_->Consume(drop_bus.get(), drop_bus->frames());
    else
      SourceCallback(0, drop_bus.get());
  }

  // Pull the data we will deliver.
  if (audio_fifo_)
    audio_fifo_->Consume(audio_bus, audio_bus->frames());
  else
    SourceCallback(0, audio_bus);

  return (state_ == PLAYING) ? audio_bus->frames() : 0;
}

void WebRtcAudioRenderer::OnRenderError() {
  NOTIMPLEMENTED();
  LOG(ERROR) << "OnRenderError()";
}

// Called by AudioPullFifo when more data is necessary.
void WebRtcAudioRenderer::SourceCallback(
    int fifo_frame_delay, media::AudioBus* audio_bus) {
  DCHECK(sink_->CurrentThreadIsRenderingThread());
  base::TimeTicks start_time = base::TimeTicks::Now();
  DVLOG(2) << "WebRtcAudioRenderer::SourceCallback("
           << fifo_frame_delay << ", "
           << audio_bus->frames() << ")";

  int output_delay_milliseconds = audio_delay_.InMilliseconds();
  // TODO(grunell): This integer division by sample_rate will cause loss of
  // partial milliseconds, and may cause avsync drift. http://crbug.com/586540
  output_delay_milliseconds += fifo_frame_delay *
                               base::Time::kMillisecondsPerSecond /
                               sink_params_.sample_rate();
  DVLOG(2) << "output_delay_milliseconds: " << output_delay_milliseconds;

  // We need to keep render data for the |source_| regardless of |state_|,
  // otherwise the data will be buffered up inside |source_|.
  source_->RenderData(audio_bus, sink_params_.sample_rate(),
                      output_delay_milliseconds,
                      &current_time_);

  // Avoid filling up the audio bus if we are not playing; instead
  // return here and ensure that the returned value in Render() is 0.
  if (state_ != PLAYING)
    audio_bus->Zero();

  // Measure the elapsed time for this function and log it to UMA. Store the max
  // value. Don't do this for low resolution clocks to not skew data.
  if (base::TimeTicks::IsHighResolution()) {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start_time;
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.Audio.Render.GetSourceDataTime.WebRTC",
                                elapsed.InMicroseconds(),
                                kRenderTimeHistogramMinMicroseconds,
                                kRenderTimeHistogramMaxMicroseconds, 50);

    if (elapsed > max_render_time_)
      max_render_time_ = elapsed;
  }
}

void WebRtcAudioRenderer::UpdateSourceVolume(
    webrtc::AudioSourceInterface* source) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note: If there are no playing audio renderers, then the volume will be
  // set to 0.0.
  float volume = 0.0f;

  SourcePlayingStates::iterator entry = source_playing_states_.find(source);
  if (entry != source_playing_states_.end()) {
    PlayingStates& states = entry->second;
    for (PlayingStates::const_iterator it = states.begin();
         it != states.end(); ++it) {
      if ((*it)->playing())
        volume += (*it)->volume();
    }
  }

  // The valid range for volume scaling of a remote webrtc source is
  // 0.0-10.0 where 1.0 is no attenuation/boost.
  DCHECK(volume >= 0.0f);
  if (volume > 10.0f)
    volume = 10.0f;

  DVLOG(1) << "Setting remote source volume: " << volume;
  if (!signaling_thread_->BelongsToCurrentThread()) {
    // Libjingle hands out proxy objects in most cases, but the audio source
    // object is an exception (bug?).  So, to work around that, we need to make
    // sure we call SetVolume on the signaling thread.
    signaling_thread_->PostTask(FROM_HERE,
        base::Bind(&webrtc::AudioSourceInterface::SetVolume, source, volume));
  } else {
    source->SetVolume(volume);
  }
}

bool WebRtcAudioRenderer::AddPlayingState(
    webrtc::AudioSourceInterface* source,
    PlayingState* state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state->playing());
  // Look up or add the |source| to the map.
  PlayingStates& array = source_playing_states_[source];
  if (std::find(array.begin(), array.end(), state) != array.end())
    return false;

  array.push_back(state);

  return true;
}

bool WebRtcAudioRenderer::RemovePlayingState(
    webrtc::AudioSourceInterface* source,
    PlayingState* state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!state->playing());
  SourcePlayingStates::iterator found = source_playing_states_.find(source);
  if (found == source_playing_states_.end())
    return false;

  PlayingStates& array = found->second;
  PlayingStates::iterator state_it =
      std::find(array.begin(), array.end(), state);
  if (state_it == array.end())
    return false;

  array.erase(state_it);

  if (array.empty())
    source_playing_states_.erase(found);

  return true;
}

void WebRtcAudioRenderer::OnPlayStateChanged(
    const blink::WebMediaStream& media_stream,
    PlayingState* state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  blink::WebVector<blink::WebMediaStreamTrack> web_tracks;
  media_stream.AudioTracks(web_tracks);

  for (const blink::WebMediaStreamTrack& web_track : web_tracks) {
    // WebRtcAudioRenderer can only render audio tracks received from a remote
    // peer. Since the actual MediaStream is mutable from JavaScript, we need
    // to make sure |web_track| is actually a remote track.
    PeerConnectionRemoteAudioTrack* const remote_track =
        PeerConnectionRemoteAudioTrack::From(
            MediaStreamAudioTrack::From(web_track));
    if (!remote_track)
      continue;
    webrtc::AudioSourceInterface* source =
        remote_track->track_interface()->GetSource();
    DCHECK(source);
    if (!state->playing()) {
      if (RemovePlayingState(source, state))
        EnterPauseState();
    } else if (AddPlayingState(source, state)) {
      EnterPlayState();
    }
    UpdateSourceVolume(source);
  }
}

void WebRtcAudioRenderer::OnPlayStateRemoved(PlayingState* state) {
  // It is possible we associated |state| to a source for a track that is no
  // longer easily reachable. We iterate over |source_playing_states_| to
  // ensure there are no dangling pointers to |state| there. See
  // crbug.com/697256.
  // TODO(maxmorin): Clean up cleanup code in this and related classes so that
  // this hack isn't necessary.
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto it = source_playing_states_.begin();
       it != source_playing_states_.end();) {
    PlayingStates& states = it->second;
    // We cannot use RemovePlayingState as it might invalidate |it|.
    states.erase(std::remove(states.begin(), states.end(), state),
                 states.end());
    if (states.empty())
      it = source_playing_states_.erase(it);
    else
      ++it;
  }
}

void WebRtcAudioRenderer::PrepareSink() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media::AudioParameters new_sink_params;
  {
    base::AutoLock lock(lock_);
    new_sink_params = sink_params_;
  }

  const media::OutputDeviceInfo& device_info = sink_->GetOutputDeviceInfo();
  DCHECK_EQ(device_info.device_status(), media::OUTPUT_DEVICE_STATUS_OK);

  // WebRTC does not yet support higher rates than 96000 on the client side
  // and 48000 is the preferred sample rate. Therefore, if 192000 is detected,
  // we change the rate to 48000 instead. The consequence is that the native
  // layer will be opened up at 192kHz but WebRTC will provide data at 48kHz
  // which will then be resampled by the audio converted on the browser side
  // to match the native audio layer.
  int sample_rate = device_info.output_params().sample_rate();
  DVLOG(1) << "Audio output hardware sample rate: " << sample_rate;
  if (sample_rate >= 192000) {
    DVLOG(1) << "Resampling from 48000 to " << sample_rate << " is required";
    sample_rate = 48000;
  }
  media::AudioSampleRate asr;
  if (media::ToAudioSampleRate(sample_rate, &asr)) {
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AudioOutputSampleRate", asr,
                              media::kAudioSampleRateMax + 1);
  } else {
    UMA_HISTOGRAM_COUNTS("WebRTC.AudioOutputSampleRateUnexpected", sample_rate);
  }

  // Calculate the frames per buffer for the source, i.e. the WebRTC client. We
  // use 10 ms of data since the WebRTC client only supports multiples of 10 ms
  // as buffer size where 10 ms is preferred for lowest possible delay.
  const int source_frames_per_buffer = (sample_rate / 100);
  DVLOG(1) << "Using WebRTC output buffer size: " << source_frames_per_buffer;

  // Setup sink parameters.
  const int sink_frames_per_buffer = media::AudioLatency::GetRtcBufferSize(
      sample_rate, device_info.output_params().frames_per_buffer());
  new_sink_params.set_sample_rate(sample_rate);
  new_sink_params.set_frames_per_buffer(sink_frames_per_buffer);

  // Create a FIFO if re-buffering is required to match the source input with
  // the sink request. The source acts as provider here and the sink as
  // consumer.
  const bool different_source_sink_frames =
      source_frames_per_buffer != new_sink_params.frames_per_buffer();
  if (different_source_sink_frames) {
    DVLOG(1) << "Rebuffering from " << source_frames_per_buffer << " to "
             << new_sink_params.frames_per_buffer();
  }
  {
    base::AutoLock lock(lock_);
    if ((!audio_fifo_ && different_source_sink_frames) ||
        (audio_fifo_ &&
         audio_fifo_->SizeInFrames() != source_frames_per_buffer)) {
      audio_fifo_.reset(new media::AudioPullFifo(
          kChannels, source_frames_per_buffer,
          base::Bind(&WebRtcAudioRenderer::SourceCallback,
                     base::Unretained(this))));
    }
    sink_params_ = new_sink_params;
  }

  // Specify the latency info to be passed to the browser side.
  new_sink_params.set_latency_tag(AudioDeviceFactory::GetSourceLatencyType(
      AudioDeviceFactory::AudioDeviceFactory::kSourceWebRtc));
  sink_->Initialize(new_sink_params, this);
}

}  // namespace content
