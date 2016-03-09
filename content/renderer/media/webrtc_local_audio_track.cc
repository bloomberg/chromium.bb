// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_track.h"

#include <stdint.h>

#include <limits>

#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/media/media_stream_audio_level_calculator.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/media_stream_audio_sink_owner.h"
#include "content/renderer/media/media_stream_audio_track_sink.h"
#include "content/renderer/media/webaudio_capturer_source.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"

namespace content {

WebRtcLocalAudioTrack::WebRtcLocalAudioTrack(
    WebRtcLocalAudioTrackAdapter* adapter,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    WebAudioCapturerSource* webaudio_source)
    : MediaStreamAudioTrack(true),
      adapter_(adapter),
      capturer_(capturer),
      webaudio_source_(webaudio_source) {
  DCHECK(capturer.get() || webaudio_source);
  signal_thread_checker_.DetachFromThread();

  adapter_->Initialize(this);

  DVLOG(1) << "WebRtcLocalAudioTrack::WebRtcLocalAudioTrack()";
}

WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack()";
  // Users might not call Stop() on the track.
  Stop();
}

media::AudioParameters WebRtcLocalAudioTrack::GetOutputFormat() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (webaudio_source_.get()) {
    return media::AudioParameters();
  } else {
    return capturer_->GetOutputFormat();
  }
}

void WebRtcLocalAudioTrack::Capture(const media::AudioBus& audio_bus,
                                    base::TimeTicks estimated_capture_time,
                                    bool force_report_nonzero_energy) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(!estimated_capture_time.is_null());

  // Calculate the signal level regardless of whether the track is disabled or
  // enabled.  If |force_report_nonzero_energy| is true, |audio_bus| contains
  // post-processed data that may be all zeros even though the signal contained
  // energy before the processing.  In this case, report nonzero energy even if
  // the energy of the data in |audio_bus| is zero.
  const float minimum_signal_level =
      force_report_nonzero_energy ? 1.0f / std::numeric_limits<int16_t>::max()
                                  : 0.0f;
  const float signal_level = std::max(
      minimum_signal_level,
      std::min(1.0f, level_calculator_->Calculate(audio_bus)));
  const int signal_level_as_pcm16 =
      static_cast<int>(signal_level * std::numeric_limits<int16_t>::max() +
                       0.5f /* rounding to nearest int */);
  adapter_->SetSignalLevel(signal_level_as_pcm16);

  scoped_refptr<WebRtcAudioCapturer> capturer;
  SinkList::ItemList sinks;
  SinkList::ItemList sinks_to_notify_format;
  {
    base::AutoLock auto_lock(lock_);
    capturer = capturer_;
    sinks = sinks_.Items();
    sinks_.RetrieveAndClearTags(&sinks_to_notify_format);
  }

  // Notify the tracks on when the format changes. This will do nothing if
  // |sinks_to_notify_format| is empty.
  for (const auto& sink : sinks_to_notify_format)
    sink->OnSetFormat(audio_parameters_);

  // Feed the data to the sinks.
  // TODO(jiayl): we should not pass the real audio data down if the track is
  // disabled. This is currently done so to feed input to WebRTC typing
  // detection and should be changed when audio processing is moved from
  // WebRTC to the track.
  for (const auto& sink : sinks)
    sink->OnData(audio_bus, estimated_capture_time);
}

void WebRtcLocalAudioTrack::OnSetFormat(
    const media::AudioParameters& params) {
  DVLOG(1) << "WebRtcLocalAudioTrack::OnSetFormat()";
  // If the source is restarted, we might have changed to another capture
  // thread.
  capture_thread_checker_.DetachFromThread();
  DCHECK(capture_thread_checker_.CalledOnValidThread());

  audio_parameters_ = params;
  level_calculator_.reset(new MediaStreamAudioLevelCalculator());

  base::AutoLock auto_lock(lock_);
  // Remember to notify all sinks of the new format.
  sinks_.TagAll();
}

void WebRtcLocalAudioTrack::SetAudioProcessor(
    const scoped_refptr<MediaStreamAudioProcessor>& processor) {
  // if the |processor| does not have audio processing, which can happen if
  // kDisableAudioTrackProcessing is set set or all the constraints in
  // the |processor| are turned off. In such case, we pass NULL to the
  // adapter to indicate that no stats can be gotten from the processor.
  adapter_->SetAudioProcessor(processor->has_audio_processing() ?
      processor : NULL);
}

void WebRtcLocalAudioTrack::AddSink(MediaStreamAudioSink* sink) {
  // This method is called from webrtc, on the signaling thread, when the local
  // description is set and from the main thread from WebMediaPlayerMS::load
  // (via WebRtcLocalAudioRenderer::Start).
  DCHECK(main_render_thread_checker_.CalledOnValidThread() ||
         signal_thread_checker_.CalledOnValidThread());
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
  sinks_.AddAndTag(sink_owner.get());
}

void WebRtcLocalAudioTrack::RemoveSink(MediaStreamAudioSink* sink) {
  // See AddSink for additional context.  When local audio is stopped from
  // webrtc, we'll be called here on the signaling thread.
  DCHECK(main_render_thread_checker_.CalledOnValidThread() ||
         signal_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveSink()";

  scoped_refptr<MediaStreamAudioTrackSink> removed_item;
  {
    base::AutoLock auto_lock(lock_);
    removed_item = sinks_.Remove(
        MediaStreamAudioTrackSink::WrapsMediaStreamSink(sink));
  }

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
    webaudio_source_->Start(this);
  } else if (capturer_.get()) {
    capturer_->AddTrack(this);
  }

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

void WebRtcLocalAudioTrack::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (adapter_.get())
    adapter_->set_enabled(enabled);
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

webrtc::AudioTrackInterface* WebRtcLocalAudioTrack::GetAudioAdapter() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  return adapter_.get();
}

}  // namespace content
