// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"

#include <utility>

#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"

namespace content {

WebRtcMediaStreamTrackAdapterMap::AdapterEntry::AdapterEntry(
    const scoped_refptr<WebRtcMediaStreamTrackAdapter>& adapter)
    : adapter(adapter) {}

WebRtcMediaStreamTrackAdapterMap::AdapterEntry::AdapterEntry(
    AdapterEntry&& other)
    : adapter(other.adapter) {
  other.adapter = nullptr;
}

WebRtcMediaStreamTrackAdapterMap::AdapterEntry::~AdapterEntry() {
}

WebRtcMediaStreamTrackAdapterMap::AdapterRef::AdapterRef(
    const scoped_refptr<WebRtcMediaStreamTrackAdapterMap>& map,
    Type type,
    const MapEntryIterator& it)
    : map_(map), type_(type), it_(it), adapter_(entry()->adapter) {
  DCHECK(map_);
  DCHECK(adapter_);
}

WebRtcMediaStreamTrackAdapterMap::AdapterRef::~AdapterRef() {
  DCHECK(map_->main_thread_->BelongsToCurrentThread());
  scoped_refptr<WebRtcMediaStreamTrackAdapter> removed_adapter;
  {
    base::AutoLock scoped_lock(map_->lock_);
    adapter_ = nullptr;
    if (entry()->adapter->HasOneRef()) {
      removed_adapter = entry()->adapter;
      if (type_ == Type::kLocal)
        map_->local_track_adapters_.erase(it_);
      else
        map_->remote_track_adapters_.erase(it_);
    }
  }
  // Dispose the adapter if it was removed. This is performed after releasing
  // the lock so that it is safe for any disposal mechanism to do synchronous
  // invokes to the signaling thread without any risk of deadlock.
  if (removed_adapter) {
    removed_adapter->Dispose();
  }
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::AdapterRef::Copy() const {
  base::AutoLock scoped_lock(map_->lock_);
  return base::WrapUnique(new AdapterRef(map_, type_, it_));
}

WebRtcMediaStreamTrackAdapterMap::WebRtcMediaStreamTrackAdapterMap(
    PeerConnectionDependencyFactory* const factory)
    : factory_(factory), main_thread_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(factory_);
  DCHECK(main_thread_);
}

WebRtcMediaStreamTrackAdapterMap::~WebRtcMediaStreamTrackAdapterMap() {
  DCHECK(local_track_adapters_.empty());
  DCHECK(remote_track_adapters_.empty());
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetLocalTrackAdapter(const std::string& id) {
  return GetTrackAdapter(AdapterRef::Type::kLocal, id);
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetOrCreateLocalTrackAdapter(
    const blink::WebMediaStreamTrack& web_track) {
  DCHECK(!web_track.IsNull());
  DCHECK(main_thread_->BelongsToCurrentThread());
  return GetOrCreateTrackAdapter(
      AdapterRef::Type::kLocal,
      base::Bind(&WebRtcMediaStreamTrackAdapter::CreateLocalTrackAdapter,
                 factory_, main_thread_, web_track),
      web_track.Id().Utf8());
}

size_t WebRtcMediaStreamTrackAdapterMap::GetLocalTrackCount() const {
  base::AutoLock scoped_lock(lock_);
  return local_track_adapters_.size();
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetRemoteTrackAdapter(const std::string& id) {
  return GetTrackAdapter(AdapterRef::Type::kRemote, id);
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetOrCreateRemoteTrackAdapter(
    webrtc::MediaStreamTrackInterface* webrtc_track) {
  DCHECK(webrtc_track);
  DCHECK(!main_thread_->BelongsToCurrentThread());
  return GetOrCreateTrackAdapter(
      AdapterRef::Type::kRemote,
      base::Bind(&WebRtcMediaStreamTrackAdapter::CreateRemoteTrackAdapter,
                 factory_, main_thread_, webrtc_track),
      webrtc_track->id());
}

size_t WebRtcMediaStreamTrackAdapterMap::GetRemoteTrackCount() const {
  base::AutoLock scoped_lock(lock_);
  return remote_track_adapters_.size();
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetTrackAdapter(
    WebRtcMediaStreamTrackAdapterMap::AdapterRef::Type type,
    const std::string& id) {
  std::map<std::string, AdapterEntry>* track_adapters =
      type == AdapterRef::Type::kLocal ? &local_track_adapters_
                                       : &remote_track_adapters_;
  base::AutoLock scoped_lock(lock_);
  auto it = track_adapters->find(id);
  if (it == track_adapters->end())
    return nullptr;
  return base::WrapUnique(new AdapterRef(this, type, it));
}

std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
WebRtcMediaStreamTrackAdapterMap::GetOrCreateTrackAdapter(
    AdapterRef::Type type,
    base::Callback<scoped_refptr<WebRtcMediaStreamTrackAdapter>()>
        create_adapter_callback,
    const std::string& id) {
  std::map<std::string, AdapterEntry>* track_adapters =
      type == AdapterRef::Type::kLocal ? &local_track_adapters_
                                       : &remote_track_adapters_;
  base::AutoLock scoped_lock(lock_);
  auto it = track_adapters->find(id);
  if (it == track_adapters->end()) {
    scoped_refptr<WebRtcMediaStreamTrackAdapter> adapter =
        create_adapter_callback.Run();
    it =
        track_adapters->insert(std::make_pair(id, AdapterEntry(adapter))).first;
  }
  return base::WrapUnique(new AdapterRef(this, type, it));
}

}  // namespace content
