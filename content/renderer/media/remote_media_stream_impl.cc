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

// Gets the adapters for the tracks that are members of the webrtc stream.
// Invoke on webrtc signaling thread. New adapters are initialized in a post
// to the main thread after which their |web_track| becomes available.
RemoteMediaStreamImpl::AdapterRefMap GetAdapterRefMapFromWebRtcStream(
    const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
    webrtc::MediaStreamInterface* webrtc_stream) {
  RemoteMediaStreamImpl::AdapterRefMap adapter_refs;
  for (auto& webrtc_audio_track : webrtc_stream->GetAudioTracks()) {
    adapter_refs.insert(
        std::make_pair(webrtc_audio_track->id(),
                       track_adapter_map->GetOrCreateRemoteTrackAdapter(
                           webrtc_audio_track.get())));
  }
  for (auto& webrtc_video_track : webrtc_stream->GetVideoTracks()) {
    adapter_refs.insert(
        std::make_pair(webrtc_video_track->id(),
                       track_adapter_map->GetOrCreateRemoteTrackAdapter(
                           webrtc_video_track.get())));
  }
  return adapter_refs;
}

}  // namespace

class RemoteMediaStreamImpl::Observer
    : public webrtc::ObserverInterface,
      public base::RefCountedThreadSafe<Observer> {
 public:
  Observer(
      const base::WeakPtr<RemoteMediaStreamImpl>& media_stream,
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
      webrtc::MediaStreamInterface* webrtc_stream)
      : media_stream_(media_stream),
        main_thread_(main_thread),
        track_adapter_map_(track_adapter_map),
        webrtc_stream_(webrtc_stream) {
    webrtc_stream_->RegisterObserver(this);
  }

  const scoped_refptr<webrtc::MediaStreamInterface>& stream() const {
    return webrtc_stream_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& main_thread() const {
    return main_thread_;
  }

  void InitializeOnMainThread(const std::string& label,
                              AdapterRefMap adapter_refs,
                              size_t audio_track_count,
                              size_t video_track_count) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (media_stream_) {
      media_stream_->InitializeOnMainThread(
          label, std::move(adapter_refs), audio_track_count, video_track_count);
    }
  }

  // Uninitializes the observer, unregisteres from receiving notifications
  // and releases the webrtc stream.
  // Note: Must be called from the main thread before releasing the main
  // reference.
  void Unregister() {
    DCHECK(main_thread_->BelongsToCurrentThread());
    webrtc_stream_->UnregisterObserver(this);
    // Since we're guaranteed to not get further notifications, it's safe to
    // release the webrtc_stream_ here.
    webrtc_stream_ = nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<Observer>;

  ~Observer() override {
    DCHECK(!webrtc_stream_.get()) << "Unregister hasn't been called";
  }

  // webrtc::ObserverInterface implementation.
  void OnChanged() override {
    AdapterRefMap new_adapter_refs = GetAdapterRefMapFromWebRtcStream(
        track_adapter_map_, webrtc_stream_.get());
    main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&RemoteMediaStreamImpl::Observer::OnChangedOnMainThread,
                       this, base::Passed(&new_adapter_refs)));
  }

  void OnChangedOnMainThread(AdapterRefMap new_adapter_refs) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    if (media_stream_)
      media_stream_->OnChanged(std::move(new_adapter_refs));
  }

  base::WeakPtr<RemoteMediaStreamImpl> media_stream_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  const scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream_;
};

// Called on the signaling thread.
RemoteMediaStreamImpl::RemoteMediaStreamImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
    const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& track_adapter_map,
    webrtc::MediaStreamInterface* webrtc_stream)
    : signaling_thread_(base::ThreadTaskRunnerHandle::Get()),
      track_adapter_map_(track_adapter_map),
      weak_factory_(this) {
  DCHECK(!main_thread->BelongsToCurrentThread());
  DCHECK(track_adapter_map_);
  observer_ = new RemoteMediaStreamImpl::Observer(
      weak_factory_.GetWeakPtr(), main_thread, track_adapter_map_,
      webrtc_stream);

  AdapterRefMap adapter_refs =
      GetAdapterRefMapFromWebRtcStream(track_adapter_map_, webrtc_stream);
  main_thread->PostTask(
      FROM_HERE,
      base::BindOnce(&RemoteMediaStreamImpl::Observer::InitializeOnMainThread,
                     observer_, webrtc_stream->label(),
                     base::Passed(&adapter_refs),
                     webrtc_stream->GetAudioTracks().size(),
                     webrtc_stream->GetVideoTracks().size()));
}

RemoteMediaStreamImpl::~RemoteMediaStreamImpl() {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
  observer_->Unregister();
  OnChanged(AdapterRefMap());
}

const scoped_refptr<webrtc::MediaStreamInterface>&
RemoteMediaStreamImpl::webrtc_stream() {
  return observer_->stream();
}

void RemoteMediaStreamImpl::InitializeOnMainThread(const std::string& label,
                                                   AdapterRefMap adapter_refs,
                                                   size_t audio_track_count,
                                                   size_t video_track_count) {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());
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

  webkit_stream_.Initialize(blink::WebString::FromUTF8(label), web_audio_tracks,
                            web_video_tracks);
  webkit_stream_.SetExtraData(new MediaStream());
}

void RemoteMediaStreamImpl::OnChanged(AdapterRefMap new_adapter_refs) {
  DCHECK(observer_->main_thread()->BelongsToCurrentThread());

  // Find removed tracks.
  for (auto it = adapter_refs_.begin(); it != adapter_refs_.end();) {
    if (new_adapter_refs.find(it->first) == new_adapter_refs.end()) {
      webkit_stream_.RemoveTrack(it->second->web_track());
      it = adapter_refs_.erase(it);
    } else {
      ++it;
    }
  }
  // Find added tracks.
  for (auto& it : new_adapter_refs) {
    if (adapter_refs_.find(it.first) == adapter_refs_.end()) {
      webkit_stream_.AddTrack(it.second->web_track());
      adapter_refs_.insert(std::make_pair(it.first, std::move(it.second)));
    }
  }
}

}  // namespace content
