// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/remote_media_stream_impl.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_track.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/remote_media_stream_track_adapter.h"
#include "content/renderer/media/webrtc/media_stream_remote_video_source.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {
namespace {

template <typename WebRtcTrackVector, typename AdapterType>
void CreateAdaptersForTracks(
    const WebRtcTrackVector& tracks,
    std::vector<scoped_refptr<AdapterType>>* observers,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread) {
  for (auto& track : tracks)
    observers->push_back(new AdapterType(main_thread, track));
}

template<typename VectorType>
bool IsTrackInVector(const VectorType& v, const std::string& id) {
  for (const auto& t : v) {
    if (t->id() == id)
      return true;
  }
  return false;
}
}  // namespace

RemoteMediaStreamImpl::Observer::Observer(
    const base::WeakPtr<RemoteMediaStreamImpl>& media_stream,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::MediaStreamInterface* webrtc_stream)
    : media_stream_(media_stream),
      main_thread_(main_thread),
      webrtc_stream_(webrtc_stream) {
  webrtc_stream_->RegisterObserver(this);
}

RemoteMediaStreamImpl::Observer::~Observer() {
  DCHECK(!webrtc_stream_.get()) << "Unregister hasn't been called";
}

void RemoteMediaStreamImpl::Observer::InitializeOnMainThread(
    const std::string& label) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (media_stream_)
    media_stream_->InitializeOnMainThread(label);
}

void RemoteMediaStreamImpl::Observer::Unregister() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  webrtc_stream_->UnregisterObserver(this);
  // Since we're guaranteed to not get further notifications, it's safe to
  // release the webrtc_stream_ here.
  webrtc_stream_ = nullptr;
}

void RemoteMediaStreamImpl::Observer::OnChanged() {
  std::unique_ptr<RemoteAudioTrackAdapters> audio(
      new RemoteAudioTrackAdapters());
  std::unique_ptr<RemoteVideoTrackAdapters> video(
      new RemoteVideoTrackAdapters());

  CreateAdaptersForTracks(
      webrtc_stream_->GetAudioTracks(), audio.get(), main_thread_);
  CreateAdaptersForTracks(
      webrtc_stream_->GetVideoTracks(), video.get(), main_thread_);

  main_thread_->PostTask(FROM_HERE,
      base::Bind(&RemoteMediaStreamImpl::Observer::OnChangedOnMainThread,
      this, base::Passed(&audio), base::Passed(&video)));
}

void RemoteMediaStreamImpl::Observer::OnChangedOnMainThread(
    std::unique_ptr<RemoteAudioTrackAdapters> audio_tracks,
    std::unique_ptr<RemoteVideoTrackAdapters> video_tracks) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  if (media_stream_)
    media_stream_->OnChanged(std::move(audio_tracks), std::move(video_tracks));
}

// Called on the signaling thread.
RemoteMediaStreamImpl::RemoteMediaStreamImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    webrtc::MediaStreamInterface* webrtc_stream)
    : signaling_thread_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  observer_ = new RemoteMediaStreamImpl::Observer(
      weak_factory_.GetWeakPtr(), main_thread, webrtc_stream);
  CreateAdaptersForTracks(webrtc_stream->GetAudioTracks(),
      &audio_track_observers_, main_thread);
  CreateAdaptersForTracks(webrtc_stream->GetVideoTracks(),
      &video_track_observers_, main_thread);

  main_thread->PostTask(FROM_HERE,
      base::Bind(&RemoteMediaStreamImpl::Observer::InitializeOnMainThread,
          observer_, webrtc_stream->label()));
}

RemoteMediaStreamImpl::~RemoteMediaStreamImpl() {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
  for (auto& track : audio_track_observers_)
    track->Unregister();
  observer_->Unregister();
}

void RemoteMediaStreamImpl::InitializeOnMainThread(const std::string& label) {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
  blink::WebVector<blink::WebMediaStreamTrack> webkit_audio_tracks(
        audio_track_observers_.size());
  for (size_t i = 0; i < audio_track_observers_.size(); ++i) {
    audio_track_observers_[i]->Initialize();
    webkit_audio_tracks[i] = *audio_track_observers_[i]->web_track();
  }

  blink::WebVector<blink::WebMediaStreamTrack> webkit_video_tracks(
        video_track_observers_.size());
  for (size_t i = 0; i < video_track_observers_.size(); ++i) {
    video_track_observers_[i]->Initialize();
    webkit_video_tracks[i] = *video_track_observers_[i]->web_track();
  }

  webkit_stream_.Initialize(blink::WebString::FromUTF8(label),
                            webkit_audio_tracks, webkit_video_tracks);
  webkit_stream_.SetExtraData(new MediaStream());
}

void RemoteMediaStreamImpl::OnChanged(
    std::unique_ptr<RemoteAudioTrackAdapters> audio_tracks,
    std::unique_ptr<RemoteVideoTrackAdapters> video_tracks) {
  // Find removed tracks.
  auto audio_it = audio_track_observers_.begin();
  while (audio_it != audio_track_observers_.end()) {
    if (!IsTrackInVector(*audio_tracks.get(), (*audio_it)->id())) {
      (*audio_it)->Unregister();
      webkit_stream_.RemoveTrack(*(*audio_it)->web_track());
      audio_it = audio_track_observers_.erase(audio_it);
    } else {
      ++audio_it;
    }
  }

  auto video_it = video_track_observers_.begin();
  while (video_it != video_track_observers_.end()) {
    if (!IsTrackInVector(*video_tracks.get(), (*video_it)->id())) {
      webkit_stream_.RemoveTrack(*(*video_it)->web_track());
      video_it = video_track_observers_.erase(video_it);
    } else {
      ++video_it;
    }
  }

  // Find added tracks.
  for (auto& track : *audio_tracks.get()) {
    if (!IsTrackInVector(audio_track_observers_, track->id())) {
      track->Initialize();
      audio_track_observers_.push_back(track);
      webkit_stream_.AddTrack(*track->web_track());
      // Set the track to null to avoid unregistering it below now that it's
      // been associated with a media stream.
      track = nullptr;
    }
  }

  // Find added video tracks.
  for (const auto& track : *video_tracks.get()) {
    if (!IsTrackInVector(video_track_observers_, track->id())) {
      track->Initialize();
      video_track_observers_.push_back(track);
      webkit_stream_.AddTrack(*track->web_track());
    }
  }

  // Unregister all the audio track observers that were not used.
  // We need to do this before destruction since the observers can't unregister
  // from within the dtor due to a race.
  for (auto& track : *audio_tracks.get()) {
    if (track.get())
      track->Unregister();
  }
}

}  // namespace content
