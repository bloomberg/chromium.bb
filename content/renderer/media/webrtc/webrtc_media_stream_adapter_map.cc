// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_media_stream_adapter_map.h"

namespace content {

WebRtcMediaStreamAdapterMap::AdapterEntry::AdapterEntry(
    std::unique_ptr<WebRtcMediaStreamAdapter> adapter)
    : adapter(std::move(adapter)), ref_count(0) {
  DCHECK(this->adapter);
}

WebRtcMediaStreamAdapterMap::AdapterEntry::AdapterEntry(AdapterEntry&& other)
    : adapter(other.adapter.release()), ref_count(other.ref_count) {}

WebRtcMediaStreamAdapterMap::AdapterEntry::~AdapterEntry() {
  // |ref_count| is allowed to be non-zero only if this entry has been moved
  // which is the case if the |adapter| has already been released.
  DCHECK(!ref_count || !adapter);
}

WebRtcMediaStreamAdapterMap::AdapterRef::AdapterRef(
    scoped_refptr<WebRtcMediaStreamAdapterMap> map,
    std::map<std::string, AdapterEntry> WebRtcMediaStreamAdapterMap::*
        stream_adapters,
    const MapEntryIterator& it)
    : map_(std::move(map)), stream_adapters_(stream_adapters), it_(it) {
  DCHECK(map_);
  DCHECK(stream_adapters_);
  DCHECK(entry()->adapter);
  ++entry()->ref_count;
}

WebRtcMediaStreamAdapterMap::AdapterRef::~AdapterRef() {
  DCHECK(map_->main_thread_->BelongsToCurrentThread());
  std::unique_ptr<WebRtcMediaStreamAdapter> removed_adapter;
  {
    base::AutoLock scoped_lock(map_->lock_);
    if (--entry()->ref_count == 0) {
      removed_adapter = std::move(entry()->adapter);
      (*map_.*stream_adapters_).erase(it_);
    }
  }
  // Destroy the adapter whilst not holding the lock so that it is safe for
  // destructors to use the signaling thread synchronously without any risk of
  // deadlock.
  removed_adapter.reset();
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::AdapterRef::Copy() const {
  base::AutoLock scoped_lock(map_->lock_);
  return base::WrapUnique(new AdapterRef(map_, stream_adapters_, it_));
}

WebRtcMediaStreamAdapterMap::WebRtcMediaStreamAdapterMap(
    PeerConnectionDependencyFactory* const factory,
    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map)
    : factory_(factory),
      main_thread_(base::ThreadTaskRunnerHandle::Get()),
      track_adapter_map_(std::move(track_adapter_map)) {
  DCHECK(factory_);
  DCHECK(main_thread_);
  DCHECK(track_adapter_map_);
}

WebRtcMediaStreamAdapterMap::~WebRtcMediaStreamAdapterMap() {
  DCHECK(main_thread_->BelongsToCurrentThread());
  DCHECK(local_stream_adapters_.empty());
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::GetLocalStreamAdapter(const std::string& id) {
  base::AutoLock scoped_lock(lock_);
  auto it = local_stream_adapters_.find(id);
  if (it == local_stream_adapters_.end())
    return nullptr;
  return base::WrapUnique(new AdapterRef(
      this, &WebRtcMediaStreamAdapterMap::local_stream_adapters_, it));
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::GetOrCreateLocalStreamAdapter(
    const blink::WebMediaStream& web_stream) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return GetOrCreateStreamAdapter(
      &WebRtcMediaStreamAdapterMap::local_stream_adapters_,
      base::Bind(&WebRtcMediaStreamAdapter::CreateLocalStreamAdapter, factory_,
                 track_adapter_map_, web_stream),
      web_stream.Id().Utf8());
}

size_t WebRtcMediaStreamAdapterMap::GetLocalStreamCount() const {
  base::AutoLock scoped_lock(lock_);
  return local_stream_adapters_.size();
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::GetRemoteStreamAdapter(const std::string& id) {
  base::AutoLock scoped_lock(lock_);
  auto it = remote_stream_adapters_.find(id);
  if (it == remote_stream_adapters_.end())
    return nullptr;
  return base::WrapUnique(new AdapterRef(
      this, &WebRtcMediaStreamAdapterMap::remote_stream_adapters_, it));
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::GetOrCreateRemoteStreamAdapter(
    scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream) {
  DCHECK(!main_thread_->BelongsToCurrentThread());
  return GetOrCreateStreamAdapter(
      &WebRtcMediaStreamAdapterMap::remote_stream_adapters_,
      base::Bind(&WebRtcMediaStreamAdapter::CreateRemoteStreamAdapter,
                 main_thread_, track_adapter_map_, webrtc_stream),
      webrtc_stream->label());
}

size_t WebRtcMediaStreamAdapterMap::GetRemoteStreamCount() const {
  base::AutoLock scoped_lock(lock_);
  return remote_stream_adapters_.size();
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::GetOrCreateStreamAdapter(
    std::map<std::string, AdapterEntry> WebRtcMediaStreamAdapterMap::*
        stream_adapters,
    base::Callback<std::unique_ptr<WebRtcMediaStreamAdapter>()>
        create_adapter_callback,
    const std::string& id) {
  base::AutoLock scoped_lock(lock_);
  auto it = (this->*stream_adapters).find(id);
  if (it == (this->*stream_adapters).end()) {
    std::unique_ptr<WebRtcMediaStreamAdapter> adapter =
        create_adapter_callback.Run();
    it = (this->*stream_adapters)
             .insert(std::make_pair(id, AdapterEntry(std::move(adapter))))
             .first;
  }
  return base::WrapUnique(new AdapterRef(this, stream_adapters, it));
}

}  // namespace content
