// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/remote_media_stream_impl.h"

#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"

namespace content {

// RemoteMediaStreamTrackObserver is responsible for listening on change
// notification on a remote webrtc MediaStreamTrack and notify WebKit.
class RemoteMediaStreamTrackObserver
    : NON_EXPORTED_BASE(public webrtc::ObserverInterface),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  RemoteMediaStreamTrackObserver(
      webrtc::MediaStreamTrackInterface* webrtc_track,
      const WebKit::WebMediaStreamTrack& webkit_track);
  virtual ~RemoteMediaStreamTrackObserver();

  webrtc::MediaStreamTrackInterface* observered_track() {
    return webrtc_track_;
  }
  const WebKit::WebMediaStreamTrack& webkit_track() { return webkit_track_; }

 private:
  // webrtc::ObserverInterface implementation.
  virtual void OnChanged() OVERRIDE;

  webrtc::MediaStreamTrackInterface::TrackState state_;
  scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track_;
  WebKit::WebMediaStreamTrack webkit_track_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamTrackObserver);
};

}  // namespace content

namespace {

void InitializeWebkitTrack(webrtc::MediaStreamTrackInterface* track,
                           WebKit::WebMediaStreamTrack* webkit_track,
                           WebKit::WebMediaStreamSource::Type type) {
  WebKit::WebMediaStreamSource webkit_source;
  WebKit::WebString webkit_track_id(UTF8ToUTF16(track->id()));

  webkit_source.initialize(webkit_track_id, type, webkit_track_id);
  webkit_track->initialize(webkit_track_id, webkit_source);
}

content::RemoteMediaStreamTrackObserver* FindTrackObserver(
    webrtc::MediaStreamTrackInterface* track,
    const ScopedVector<content::RemoteMediaStreamTrackObserver>& observers) {
  ScopedVector<content::RemoteMediaStreamTrackObserver>::const_iterator it =
      observers.begin();
  for (; it != observers.end(); ++it) {
    if ((*it)->observered_track() == track)
      return *it;
  }
  return NULL;
}

} // namespace anonymous

namespace content {

RemoteMediaStreamTrackObserver::RemoteMediaStreamTrackObserver(
    webrtc::MediaStreamTrackInterface* webrtc_track,
    const WebKit::WebMediaStreamTrack& webkit_track)
    : state_(webrtc_track->state()),
      webrtc_track_(webrtc_track),
      webkit_track_(webkit_track) {
  webrtc_track->RegisterObserver(this);
}

RemoteMediaStreamTrackObserver::~RemoteMediaStreamTrackObserver() {
  webrtc_track_->UnregisterObserver(this);
}

void RemoteMediaStreamTrackObserver::OnChanged() {
  DCHECK(CalledOnValidThread());

  webrtc::MediaStreamTrackInterface::TrackState state = webrtc_track_->state();
  if (state == state_)
    return;

  state_ = state;
  switch (state) {
    case webrtc::MediaStreamTrackInterface::kInitializing:
      // Ignore the kInitializing state since there is no match in
      // WebMediaStreamSource::ReadyState.
      break;
    case webrtc::MediaStreamTrackInterface::kLive:
      webkit_track_.source().setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateLive);
      break;
    case webrtc::MediaStreamTrackInterface::kEnded:
      webkit_track_.source().setReadyState(
          WebKit::WebMediaStreamSource::ReadyStateEnded);
      break;
    default:
      NOTREACHED();
      break;
  }
}

RemoteMediaStreamImpl::RemoteMediaStreamImpl(
    webrtc::MediaStreamInterface* webrtc_stream)
    : webrtc_stream_(webrtc_stream) {
  webrtc_stream_->RegisterObserver(this);

  webrtc::AudioTrackVector webrtc_audio_tracks =
      webrtc_stream_->GetAudioTracks();
  WebKit::WebVector<WebKit::WebMediaStreamTrack> webkit_audio_tracks(
      webrtc_audio_tracks.size());

  // Initialize WebKit audio tracks.
  size_t i = 0;
  for (; i < webrtc_audio_tracks.size(); ++i) {
    webrtc::AudioTrackInterface* audio_track = webrtc_audio_tracks[i];
    DCHECK(audio_track);
    InitializeWebkitTrack(audio_track,  &webkit_audio_tracks[i],
                          WebKit::WebMediaStreamSource::TypeAudio);
    audio_track_observers_.push_back(
        new RemoteMediaStreamTrackObserver(audio_track,
                                           webkit_audio_tracks[i]));
  }

  // Initialize WebKit video tracks.
  webrtc::VideoTrackVector webrtc_video_tracks =
        webrtc_stream_->GetVideoTracks();
  WebKit::WebVector<WebKit::WebMediaStreamTrack> webkit_video_tracks(
       webrtc_video_tracks.size());
  for (i = 0; i < webrtc_video_tracks.size(); ++i) {
    webrtc::VideoTrackInterface* video_track = webrtc_video_tracks[i];
    DCHECK(video_track);
    InitializeWebkitTrack(video_track,  &webkit_video_tracks[i],
                          WebKit::WebMediaStreamSource::TypeVideo);
    video_track_observers_.push_back(
        new RemoteMediaStreamTrackObserver(video_track,
                                           webkit_video_tracks[i]));
  }

  webkit_stream_.initialize(UTF8ToUTF16(webrtc_stream->label()),
                            webkit_audio_tracks, webkit_video_tracks);
  webkit_stream_.setExtraData(new MediaStreamExtraData(webrtc_stream, false));
}

RemoteMediaStreamImpl::~RemoteMediaStreamImpl() {
  webrtc_stream_->UnregisterObserver(this);
}

void RemoteMediaStreamImpl::OnChanged() {
  // Find removed audio tracks.
  ScopedVector<RemoteMediaStreamTrackObserver>::iterator audio_it =
      audio_track_observers_.begin();
  while (audio_it != audio_track_observers_.end()) {
    std::string track_id = (*audio_it)->observered_track()->id();
    if (webrtc_stream_->FindAudioTrack(track_id) == NULL) {
       webkit_stream_.removeTrack((*audio_it)->webkit_track());
       audio_it = audio_track_observers_.erase(audio_it);
    } else {
      ++audio_it;
    }
  }

  // Find removed video tracks.
  ScopedVector<RemoteMediaStreamTrackObserver>::iterator video_it =
      video_track_observers_.begin();
  while (video_it != video_track_observers_.end()) {
    std::string track_id = (*video_it)->observered_track()->id();
    if (webrtc_stream_->FindVideoTrack(track_id) == NULL) {
      webkit_stream_.removeTrack((*video_it)->webkit_track());
      video_it = video_track_observers_.erase(video_it);
    } else {
      ++video_it;
    }
  }

  // Find added audio tracks.
  webrtc::AudioTrackVector webrtc_audio_tracks =
      webrtc_stream_->GetAudioTracks();
  for (webrtc::AudioTrackVector::iterator it = webrtc_audio_tracks.begin();
       it != webrtc_audio_tracks.end(); ++it) {
    if (!FindTrackObserver(*it, audio_track_observers_)) {
      WebKit::WebMediaStreamTrack new_track;
      InitializeWebkitTrack(*it, &new_track,
                            WebKit::WebMediaStreamSource::TypeAudio);
      audio_track_observers_.push_back(
          new RemoteMediaStreamTrackObserver(*it, new_track));
      webkit_stream_.addTrack(new_track);
    }
  }

  // Find added video tracks.
  webrtc::VideoTrackVector webrtc_video_tracks =
      webrtc_stream_->GetVideoTracks();
  for (webrtc::VideoTrackVector::iterator it = webrtc_video_tracks.begin();
       it != webrtc_video_tracks.end(); ++it) {
    if (!FindTrackObserver(*it, video_track_observers_)) {
      WebKit::WebMediaStreamTrack new_track;
      InitializeWebkitTrack(*it, &new_track,
                            WebKit::WebMediaStreamSource::TypeVideo);
      video_track_observers_.push_back(
          new RemoteMediaStreamTrackObserver(*it, new_track));
      webkit_stream_.addTrack(new_track);
    }
  }
}

}  // namespace content
