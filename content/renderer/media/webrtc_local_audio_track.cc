// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_track.h"

#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/media/media_stream_audio_sink_owner.h"
#include "content/renderer/media/media_stream_audio_track_sink.h"
#include "content/renderer/media/peer_connection_audio_sink_owner.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "media/base/audio_fifo.h"
#include "third_party/libjingle/source/talk/media/base/audiorenderer.h"

namespace content {

static const size_t kMaxNumberOfBuffersInFifo = 2;
static const char kAudioTrackKind[] = "audio";

namespace {

using webrtc::MediaConstraintsInterface;

// This helper function checks if any audio constraints are set that require
// audio processing to be applied.  Right now this is a big, single switch for
// all of the properties, but in the future they'll be handled one by one.
bool NeedsAudioProcessing(
    const webrtc::MediaConstraintsInterface* constraints) {
  if (!constraints)
    return false;

  static const char* kAudioProcessingProperties[] = {
    MediaConstraintsInterface::kEchoCancellation,
    MediaConstraintsInterface::kExperimentalEchoCancellation,
    MediaConstraintsInterface::kAutoGainControl,
    MediaConstraintsInterface::kExperimentalAutoGainControl,
    MediaConstraintsInterface::kNoiseSuppression,
    MediaConstraintsInterface::kHighpassFilter,
    MediaConstraintsInterface::kTypingNoiseDetection,
  };

  for (size_t i = 0; i < arraysize(kAudioProcessingProperties); ++i) {
    bool value = false;
    if (webrtc::FindConstraint(constraints, kAudioProcessingProperties[i],
                               &value, NULL) &&
        value) {
      return true;
    }
  }

  return false;
}

}  // namespace.

// This is a temporary audio buffer with parameters used to send data to
// callbacks.
class WebRtcLocalAudioTrack::ConfiguredBuffer {
 public:
  ConfiguredBuffer() {}
  virtual ~ConfiguredBuffer() {}

  void Configure(const media::AudioParameters& params) {
    DCHECK(params.IsValid());

    // PeerConnection uses 10ms as the sink buffer size as its native packet
    // size. We use the native PeerConnection buffer size to achieve the best
    // performance when a PeerConnection is connected with a track.
    int sink_buffer_size = params.sample_rate() / 100;
    if (params.frames_per_buffer() < sink_buffer_size) {
      // When the source is running with a buffer size smaller than the peer
      // connection buffer size, that means no PeerConnection is connected
      // to the track, use the same buffer size as the incoming format to
      // avoid extra FIFO for WebAudio.
      sink_buffer_size = params.frames_per_buffer();
    }
    params_.Reset(params.format(), params.channel_layout(), params.channels(),
                  params.input_channels(), params.sample_rate(),
                  params.bits_per_sample(), sink_buffer_size);

    audio_wrapper_ = media::AudioBus::Create(params_.channels(),
                                             params_.frames_per_buffer());
    buffer_.reset(new int16[params_.frames_per_buffer() * params_.channels()]);

    // The size of the FIFO should be at least twice of the source buffer size
    // or twice of the sink buffer size.
    int buffer_size = std::max(
        kMaxNumberOfBuffersInFifo * params.frames_per_buffer(),
        kMaxNumberOfBuffersInFifo * params_.frames_per_buffer());
    fifo_.reset(new media::AudioFifo(params_.channels(), buffer_size));
  }

  void Push(media::AudioBus* audio_source) {
    DCHECK(fifo_->frames() + audio_source->frames() <= fifo_->max_frames());
    fifo_->Push(audio_source);
  }

  bool Consume() {
    if (fifo_->frames() < audio_wrapper_->frames())
      return false;

    fifo_->Consume(audio_wrapper_.get(), 0, audio_wrapper_->frames());
    audio_wrapper_->ToInterleaved(audio_wrapper_->frames(),
                                  params_.bits_per_sample() / 8,
                                  buffer());
    return true;
  }

  int16* buffer() const { return buffer_.get(); }

  // Format of the output audio buffer.
  const media::AudioParameters& params() const { return params_; }

 private:
  media::AudioParameters params_;
  scoped_ptr<media::AudioBus> audio_wrapper_;
  scoped_ptr<media::AudioFifo> fifo_;
  scoped_ptr<int16[]> buffer_;
};

scoped_refptr<WebRtcLocalAudioTrack> WebRtcLocalAudioTrack::Create(
    const std::string& id,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    WebAudioCapturerSource* webaudio_source,
    webrtc::AudioSourceInterface* track_source,
    const webrtc::MediaConstraintsInterface* constraints) {
  talk_base::RefCountedObject<WebRtcLocalAudioTrack>* track =
      new talk_base::RefCountedObject<WebRtcLocalAudioTrack>(
          id, capturer, webaudio_source, track_source, constraints);
  return track;
}

WebRtcLocalAudioTrack::WebRtcLocalAudioTrack(
    const std::string& label,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    WebAudioCapturerSource* webaudio_source,
    webrtc::AudioSourceInterface* track_source,
    const webrtc::MediaConstraintsInterface* constraints)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      capturer_(capturer),
      webaudio_source_(webaudio_source),
      track_source_(track_source),
      need_audio_processing_(NeedsAudioProcessing(constraints)),
      buffer_(new ConfiguredBuffer()) {
  DCHECK(capturer.get() || webaudio_source);
  if (!webaudio_source_) {
    source_provider_.reset(new WebRtcLocalAudioSourceProvider());
    AddSink(source_provider_.get());
  }
  DVLOG(1) << "WebRtcLocalAudioTrack::WebRtcLocalAudioTrack()";
}

WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack()";
  // Users might not call Stop() on the track.
  Stop();
}

void WebRtcLocalAudioTrack::Capture(media::AudioBus* audio_source,
                                    int audio_delay_milliseconds,
                                    int volume,
                                    bool key_pressed) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  scoped_refptr<WebRtcAudioCapturer> capturer;
  std::vector<int> voe_channels;
  SinkList::ItemList sinks;
  SinkList::ItemList sinks_to_notify_format;
  bool is_webaudio_source = false;
  {
    base::AutoLock auto_lock(lock_);
    capturer = capturer_;
    voe_channels = voe_channels_;
    sinks = sinks_.Items();
    sinks_.RetrieveAndClearTags(&sinks_to_notify_format);
    is_webaudio_source = (webaudio_source_.get() != NULL);
  }

  // Notify the tracks on when the format changes. This will do nothing if
  // |sinks_to_notify_format| is empty.
  for (SinkList::ItemList::const_iterator it = sinks_to_notify_format.begin();
       it != sinks_to_notify_format.end(); ++it) {
    (*it)->OnSetFormat(buffer_->params());
  }

  // Push the data to the fifo.
  buffer_->Push(audio_source);

  // When the source is WebAudio, turn off the audio processing if the delay
  // value is 0 even though the constraint is set to true. In such case, it
  // indicates the data is not from microphone.
  // TODO(xians): remove the flag when supporting one APM per audio track.
  // See crbug/264611 for details.
  bool need_audio_processing = need_audio_processing_;
  if (is_webaudio_source && need_audio_processing)
    need_audio_processing = (audio_delay_milliseconds != 0);

  int current_volume = volume;
  while (buffer_->Consume()) {
    // Feed the data to the sinks.
    // TODO (jiayl): we should not pass the real audio data down if the track is
    // disabled. This is currently done so to feed input to WebRTC typing
    // detection and should be changed when audio processing is moved from
    // WebRTC to the track.
    for (SinkList::ItemList::const_iterator it = sinks.begin();
         it != sinks.end();
         ++it) {
      int new_volume = (*it)->OnData(buffer_->buffer(),
                                     buffer_->params().sample_rate(),
                                     buffer_->params().channels(),
                                     buffer_->params().frames_per_buffer(),
                                     voe_channels,
                                     audio_delay_milliseconds,
                                     current_volume,
                                     need_audio_processing,
                                     key_pressed);
      if (new_volume != 0 && capturer.get()) {
        // Feed the new volume to WebRtc while changing the volume on the
        // browser.
        capturer->SetVolume(new_volume);
        current_volume = new_volume;
      }
    }
  }
}

void WebRtcLocalAudioTrack::OnSetFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcLocalAudioTrack::OnSetFormat()";
  // If the source is restarted, we might have changed to another capture
  // thread.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  DCHECK(params.IsValid());
  buffer_->Configure(params);

  base::AutoLock auto_lock(lock_);
  // Remember to notify all sinks of the new format.
  sinks_.TagAll();
}

void WebRtcLocalAudioTrack::AddChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::AddChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  if (std::find(voe_channels_.begin(), voe_channels_.end(), channel_id) !=
      voe_channels_.end()) {
    // We need to handle the case when the same channel is connected to the
    // track more than once.
    return;
  }

  voe_channels_.push_back(channel_id);
}

void WebRtcLocalAudioTrack::RemoveChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  std::vector<int>::iterator iter =
      std::find(voe_channels_.begin(), voe_channels_.end(), channel_id);
  DCHECK(iter != voe_channels_.end());
  voe_channels_.erase(iter);
}

// webrtc::AudioTrackInterface implementation.
webrtc::AudioSourceInterface* WebRtcLocalAudioTrack::GetSource() const {
  return track_source_;
}

cricket::AudioRenderer* WebRtcLocalAudioTrack::GetRenderer() {
  return this;
}

std::string WebRtcLocalAudioTrack::kind() const {
  return kAudioTrackKind;
}

void WebRtcLocalAudioTrack::AddSink(MediaStreamAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::AddSink()";
  base::AutoLock auto_lock(lock_);

  // Verify that |sink| is not already added to the list.
  DCHECK(!sinks_.Contains(
      MediaStreamAudioTrackSink::WrapsMediaStreamSink(sink)));

  // Create (and add to the list) a new MediaStreamAudioTrackSink
  // which owns the |sink| and delagates all calls to the
  // MediaStreamAudioSink interface. It will be tagged in the list, so
  // we remember to call OnSetFormat() on the new sink.
  scoped_refptr<MediaStreamAudioTrackSink> sink_owner(
      new MediaStreamAudioSinkOwner(sink));
  sinks_.AddAndTag(sink_owner);
}

void WebRtcLocalAudioTrack::RemoveSink(MediaStreamAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveSink()";

  base::AutoLock auto_lock(lock_);

  scoped_refptr<MediaStreamAudioTrackSink> removed_item = sinks_.Remove(
      MediaStreamAudioTrackSink::WrapsMediaStreamSink(sink));

  // Clear the delegate to ensure that no more capture callbacks will
  // be sent to this sink. Also avoids a possible crash which can happen
  // if this method is called while capturing is active.
  if (removed_item.get())
    removed_item->Reset();
}

void WebRtcLocalAudioTrack::AddSink(PeerConnectionAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::AddSink()";
  base::AutoLock auto_lock(lock_);

  // Verify that |sink| is not already added to the list.
  DCHECK(!sinks_.Contains(
      MediaStreamAudioTrackSink::WrapsPeerConnectionSink(sink)));

  // Create (and add to the list) a new MediaStreamAudioTrackSink
  // which owns the |sink| and delagates all calls to the
  // MediaStreamAudioSink interface. It will be tagged in the list, so
  // we remember to call OnSetFormat() on the new sink.
  scoped_refptr<MediaStreamAudioTrackSink> sink_owner(
      new PeerConnectionAudioSinkOwner(sink));
  sinks_.AddAndTag(sink_owner);
}

void WebRtcLocalAudioTrack::RemoveSink(PeerConnectionAudioSink* sink) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveSink()";

  base::AutoLock auto_lock(lock_);

  scoped_refptr<MediaStreamAudioTrackSink> removed_item = sinks_.Remove(
      MediaStreamAudioTrackSink::WrapsPeerConnectionSink(sink));
  // Clear the delegate to ensure that no more capture callbacks will
  // be sent to this sink. Also avoids a possible crash which can happen
  // if this method is called while capturing is active.
  if (removed_item.get())
    removed_item->Reset();
}

void WebRtcLocalAudioTrack::Start() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::Start()";
  if (webaudio_source_.get()) {
    // If the track is hooking up with WebAudio, do NOT add the track to the
    // capturer as its sink otherwise two streams in different clock will be
    // pushed through the same track.
    webaudio_source_->Start(this, capturer_.get());
    return;
  }

  if (capturer_.get())
    capturer_->AddTrack(this);

  SinkList::ItemList sinks;
  {
    base::AutoLock auto_lock(lock_);
    sinks = sinks_.Items();
  }
  for (SinkList::ItemList::const_iterator it = sinks.begin();
       it != sinks.end();
       ++it) {
    (*it)->OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateLive);
  }
}

void WebRtcLocalAudioTrack::Stop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::Stop()";
  if (!capturer_.get() && !webaudio_source_.get())
    return;

  if (webaudio_source_.get()) {
    // Called Stop() on the |webaudio_source_| explicitly so that
    // |webaudio_source_| won't push more data to the track anymore.
    // Also note that the track is not registered as a sink to the |capturer_|
    // in such case and no need to call RemoveTrack().
    webaudio_source_->Stop();
  } else {
    // It is necessary to call RemoveTrack on the |capturer_| to avoid getting
    // audio callback after Stop().
    capturer_->RemoveTrack(this);
  }

  // Protect the pointers using the lock when accessing |sinks_| and
  // setting the |capturer_| to NULL.
  SinkList::ItemList sinks;
  {
    base::AutoLock auto_lock(lock_);
    sinks = sinks_.Items();
    sinks_.Clear();
    webaudio_source_ = NULL;
    capturer_ = NULL;
  }

  for (SinkList::ItemList::const_iterator it = sinks.begin();
       it != sinks.end();
       ++it){
    (*it)->OnReadyStateChanged(blink::WebMediaStreamSource::ReadyStateEnded);
    (*it)->Reset();
  }
}

}  // namespace content
