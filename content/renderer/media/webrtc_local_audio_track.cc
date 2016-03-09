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
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"

namespace content {

WebRtcLocalAudioTrack::WebRtcLocalAudioTrack(
    scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter)
    : MediaStreamAudioTrack(true), adapter_(std::move(adapter)) {
  signal_thread_checker_.DetachFromThread();
  DVLOG(1) << "WebRtcLocalAudioTrack::WebRtcLocalAudioTrack()";

  adapter_->Initialize(this);
}

WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack()";
  // Ensure the track is stopped.
  MediaStreamAudioTrack::Stop();
}

media::AudioParameters WebRtcLocalAudioTrack::GetOutputFormat() const {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return audio_parameters_;
}

void WebRtcLocalAudioTrack::Capture(const media::AudioBus& audio_bus,
                                    base::TimeTicks estimated_capture_time) {
  DCHECK(capture_thread_checker_.CalledOnValidThread());
  DCHECK(!estimated_capture_time.is_null());

  SinkList::ItemList sinks;
  SinkList::ItemList sinks_to_notify_format;
  {
    base::AutoLock auto_lock(lock_);
    sinks = sinks_.Items();
    sinks_.RetrieveAndClearTags(&sinks_to_notify_format);
  }

  // Notify the tracks on when the format changes. This will do nothing if
  // |sinks_to_notify_format| is empty. Note that accessing |audio_parameters_|
  // without holding the |lock_| is valid since |audio_parameters_| is only
  // changed on the current thread.
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

  base::AutoLock auto_lock(lock_);
  audio_parameters_ = params;
  // Remember to notify all sinks of the new format.
  sinks_.TagAll();
}

void WebRtcLocalAudioTrack::SetLevel(
    scoped_refptr<MediaStreamAudioLevelCalculator::Level> level) {
  adapter_->SetLevel(std::move(level));
}

void WebRtcLocalAudioTrack::SetAudioProcessor(
    scoped_refptr<MediaStreamAudioProcessor> processor) {
  adapter_->SetAudioProcessor(std::move(processor));
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

void WebRtcLocalAudioTrack::SetEnabled(bool enabled) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  if (adapter_.get())
    adapter_->set_enabled(enabled);
}

void WebRtcLocalAudioTrack::OnStop() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::OnStop()";

  // Protect the pointers using the lock when accessing |sinks_|.
  SinkList::ItemList sinks;
  {
    base::AutoLock auto_lock(lock_);
    sinks = sinks_.Items();
    sinks_.Clear();
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
