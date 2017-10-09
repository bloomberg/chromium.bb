// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/renderer/media/media_stream_audio_track.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace content {

// static
std::unique_ptr<WebRtcMediaStreamAdapter>
WebRtcMediaStreamAdapter::CreateLocalStreamAdapter(
    PeerConnectionDependencyFactory* factory,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    const blink::WebMediaStream& web_stream) {
  return base::MakeUnique<LocalWebRtcMediaStreamAdapter>(
      factory, std::move(track_adapter_map), web_stream);
}

// static
std::unique_ptr<WebRtcMediaStreamAdapter>
WebRtcMediaStreamAdapter::CreateRemoteStreamAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream) {
  return base::MakeUnique<RemoteWebRtcMediaStreamAdapter>(
      std::move(main_thread), std::move(track_adapter_map),
      std::move(webrtc_stream));
}

WebRtcMediaStreamAdapter::WebRtcMediaStreamAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream,
    const blink::WebMediaStream& web_stream)
    : main_thread_(std::move(main_thread)),
      track_adapter_map_(std::move(track_adapter_map)),
      webrtc_stream_(std::move(webrtc_stream)),
      web_stream_(web_stream) {
  DCHECK(main_thread_);
  DCHECK(track_adapter_map_);
}

WebRtcMediaStreamAdapter::~WebRtcMediaStreamAdapter() {
  DCHECK(main_thread_->BelongsToCurrentThread());
}

LocalWebRtcMediaStreamAdapter::LocalWebRtcMediaStreamAdapter(
    PeerConnectionDependencyFactory* factory,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    const blink::WebMediaStream& web_stream)
    : WebRtcMediaStreamAdapter(base::ThreadTaskRunnerHandle::Get(),
                               std::move(track_adapter_map),
                               nullptr,
                               web_stream),
      factory_(factory) {
  webrtc_stream_ = factory_->CreateLocalMediaStream(web_stream.Id().Utf8());

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.AudioTracks(audio_tracks);
  for (blink::WebMediaStreamTrack& audio_track : audio_tracks)
    TrackAdded(audio_track);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.VideoTracks(video_tracks);
  for (blink::WebMediaStreamTrack& video_track : video_tracks)
    TrackAdded(video_track);

  web_stream_.AddObserver(this);
}

LocalWebRtcMediaStreamAdapter::~LocalWebRtcMediaStreamAdapter() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  web_stream_.RemoveObserver(this);

  blink::WebVector<blink::WebMediaStreamTrack> audio_tracks;
  web_stream_.AudioTracks(audio_tracks);
  for (blink::WebMediaStreamTrack& audio_track : audio_tracks)
    TrackRemoved(audio_track);

  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  web_stream_.VideoTracks(video_tracks);
  for (blink::WebMediaStreamTrack& video_track : video_tracks)
    TrackRemoved(video_track);
}

bool LocalWebRtcMediaStreamAdapter::is_initialized() const {
  return true;
}

const scoped_refptr<webrtc::MediaStreamInterface>&
LocalWebRtcMediaStreamAdapter::webrtc_stream() const {
  return webrtc_stream_;
}

const blink::WebMediaStream& LocalWebRtcMediaStreamAdapter::web_stream() const {
  return web_stream_;
}

void LocalWebRtcMediaStreamAdapter::TrackAdded(
    const blink::WebMediaStreamTrack& web_track) {
  DCHECK(adapter_refs_.find(web_track.UniqueId()) == adapter_refs_.end());
  bool is_audio_track =
      (web_track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio);
  if (is_audio_track && !MediaStreamAudioTrack::From(web_track)) {
    DLOG(ERROR) << "No native track for blink audio track.";
    return;
  }
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> adapter_ref =
      track_adapter_map_->GetOrCreateLocalTrackAdapter(web_track);
  if (is_audio_track) {
    webrtc_stream_->AddTrack(
        static_cast<webrtc::AudioTrackInterface*>(adapter_ref->webrtc_track()));
  } else {
    webrtc_stream_->AddTrack(
        static_cast<webrtc::VideoTrackInterface*>(adapter_ref->webrtc_track()));
  }
  adapter_refs_.insert(
      std::make_pair(web_track.UniqueId(), std::move(adapter_ref)));
}

void LocalWebRtcMediaStreamAdapter::TrackRemoved(
    const blink::WebMediaStreamTrack& web_track) {
  auto it = adapter_refs_.find(web_track.UniqueId());
  if (it == adapter_refs_.end()) {
    // This can happen for audio tracks that don't have a source, these would
    // never be added in the first place.
    return;
  }
  if (web_track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio) {
    webrtc_stream_->RemoveTrack(
        static_cast<webrtc::AudioTrackInterface*>(it->second->webrtc_track()));
  } else {
    webrtc_stream_->RemoveTrack(
        static_cast<webrtc::VideoTrackInterface*>(it->second->webrtc_track()));
  }
  adapter_refs_.erase(it);
}

class RemoteWebRtcMediaStreamAdapter::WebRtcStreamObserver
    : public webrtc::ObserverInterface,
      public base::RefCountedThreadSafe<WebRtcStreamObserver> {
 public:
  WebRtcStreamObserver(
      base::WeakPtr<RemoteWebRtcMediaStreamAdapter> adapter,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
      scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream)
      : adapter_(adapter),
        main_thread_(std::move(main_thread)),
        track_adapter_map_(std::move(track_adapter_map)),
        webrtc_stream_(std::move(webrtc_stream)) {
    webrtc_stream_->RegisterObserver(this);
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& main_thread() const {
    return main_thread_;
  }

  const scoped_refptr<webrtc::MediaStreamInterface>& webrtc_stream() const {
    return webrtc_stream_;
  }

  void InitializeOnMainThread(const std::string& label,
                              RemoteAdapterRefMap adapter_refs,
                              size_t audio_track_count,
                              size_t video_track_count) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (adapter_) {
      adapter_->InitializeOnMainThread(label, std::move(adapter_refs),
                                       audio_track_count, video_track_count);
    }
  }

  // Uninitializes the observer, unregisters from receiving notifications and
  // releases the webrtc stream. Must be called from the main thread before
  // releasing the main reference.
  void Unregister() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    webrtc_stream_->UnregisterObserver(this);
    // Since we're guaranteed to not get further notifications, it's safe to
    // release the webrtc_stream_ here.
    webrtc_stream_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<WebRtcStreamObserver>;

  ~WebRtcStreamObserver() override {
    DCHECK(!webrtc_stream_.get()) << "Unregister hasn't been called";
  }

  // |webrtc::ObserverInterface| implementation.
  void OnChanged() override {
    RemoteAdapterRefMap new_adapter_refs =
        RemoteWebRtcMediaStreamAdapter::GetRemoteAdapterRefMapFromWebRtcStream(
            track_adapter_map_, webrtc_stream_.get());
    main_thread_->PostTask(
        FROM_HERE, base::BindOnce(&WebRtcStreamObserver::OnChangedOnMainThread,
                                  this, base::Passed(&new_adapter_refs)));
  }

  void OnChangedOnMainThread(RemoteAdapterRefMap new_adapter_refs) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (adapter_)
      adapter_->OnChanged(std::move(new_adapter_refs));
  }

  base::WeakPtr<RemoteWebRtcMediaStreamAdapter> adapter_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream_;
};

// static
RemoteWebRtcMediaStreamAdapter::RemoteAdapterRefMap
RemoteWebRtcMediaStreamAdapter::GetRemoteAdapterRefMapFromWebRtcStream(
    const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
    webrtc::MediaStreamInterface* webrtc_stream) {
  RemoteAdapterRefMap adapter_refs;
  for (auto& webrtc_audio_track : webrtc_stream->GetAudioTracks()) {
    DCHECK(adapter_refs.find(webrtc_audio_track.get()) == adapter_refs.end());
    adapter_refs.insert(
        std::make_pair(webrtc_audio_track.get(),
                       track_adapter_map->GetOrCreateRemoteTrackAdapter(
                           webrtc_audio_track.get())));
  }
  for (auto& webrtc_video_track : webrtc_stream->GetVideoTracks()) {
    DCHECK(adapter_refs.find(webrtc_video_track.get()) == adapter_refs.end());
    adapter_refs.insert(
        std::make_pair(webrtc_video_track.get(),
                       track_adapter_map->GetOrCreateRemoteTrackAdapter(
                           webrtc_video_track.get())));
  }
  return adapter_refs;
}

RemoteWebRtcMediaStreamAdapter::RemoteWebRtcMediaStreamAdapter(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream)
    : WebRtcMediaStreamAdapter(std::move(main_thread),
                               std::move(track_adapter_map),
                               std::move(webrtc_stream),
                               // "null" |WebMediaStream|
                               blink::WebMediaStream()),
      is_initialized_(false),
      weak_factory_(this) {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  DCHECK(track_adapter_map_);
  observer_ = new RemoteWebRtcMediaStreamAdapter::WebRtcStreamObserver(
      weak_factory_.GetWeakPtr(), main_thread_, track_adapter_map_,
      webrtc_stream_);

  RemoteAdapterRefMap adapter_refs = GetRemoteAdapterRefMapFromWebRtcStream(
      track_adapter_map_, webrtc_stream_.get());
  main_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteWebRtcMediaStreamAdapter::WebRtcStreamObserver::
                         InitializeOnMainThread,
                     observer_, webrtc_stream_->label(),
                     base::Passed(&adapter_refs),
                     webrtc_stream_->GetAudioTracks().size(),
                     webrtc_stream_->GetVideoTracks().size()));
}

RemoteWebRtcMediaStreamAdapter::~RemoteWebRtcMediaStreamAdapter() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  observer_->Unregister();
  OnChanged(RemoteAdapterRefMap());
}

bool RemoteWebRtcMediaStreamAdapter::is_initialized() const {
  base::AutoLock scoped_lock(lock_);
  return is_initialized_;
}

const scoped_refptr<webrtc::MediaStreamInterface>&
RemoteWebRtcMediaStreamAdapter::webrtc_stream() const {
  base::AutoLock scoped_lock(lock_);
  DCHECK(is_initialized_);
  return webrtc_stream_;
}

const blink::WebMediaStream& RemoteWebRtcMediaStreamAdapter::web_stream()
    const {
  base::AutoLock scoped_lock(lock_);
  DCHECK(is_initialized_);
  return web_stream_;
}

void RemoteWebRtcMediaStreamAdapter::InitializeOnMainThread(
    const std::string& label,
    RemoteAdapterRefMap adapter_refs,
    size_t audio_track_count,
    size_t video_track_count) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK_EQ(audio_track_count + video_track_count, adapter_refs.size());

  adapter_refs_ = std::move(adapter_refs);
  blink::WebVector<blink::WebMediaStreamTrack> web_audio_tracks(
      audio_track_count);
  blink::WebVector<blink::WebMediaStreamTrack> web_video_tracks(
      video_track_count);
  size_t audio_i = 0;
  size_t video_i = 0;
  for (const auto& it : adapter_refs_) {
    const blink::WebMediaStreamTrack& web_track = it.second->web_track();
    if (web_track.Source().GetType() == blink::WebMediaStreamSource::kTypeAudio)
      web_audio_tracks[audio_i++] = web_track;
    else
      web_video_tracks[video_i++] = web_track;
  }

  web_stream_.Initialize(blink::WebString::FromUTF8(label), web_audio_tracks,
                         web_video_tracks);
  webrtc_stream_ = observer_->webrtc_stream();

  base::AutoLock scoped_lock(lock_);
  is_initialized_ = true;
}

void RemoteWebRtcMediaStreamAdapter::OnChanged(
    RemoteAdapterRefMap new_adapter_refs) {
  DCHECK(main_thread_->BelongsToCurrentThread());

  // Find removed tracks.
  for (auto it = adapter_refs_.begin(); it != adapter_refs_.end();) {
    if (new_adapter_refs.find(it->first) == new_adapter_refs.end()) {
      web_stream_.RemoveTrack(it->second->web_track());
      it = adapter_refs_.erase(it);
    } else {
      ++it;
    }
  }
  // Find added tracks.
  for (auto& it : new_adapter_refs) {
    if (adapter_refs_.find(it.first) == adapter_refs_.end()) {
      web_stream_.AddTrack(it.second->web_track());
      adapter_refs_.insert(std::make_pair(it.first, std::move(it.second)));
    }
  }
}

}  // namespace content
