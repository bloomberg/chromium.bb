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
    const MapEntryIterator& it)
    : map_(std::move(map)), it_(it) {
  DCHECK(map_);
  DCHECK(map_->main_thread_->BelongsToCurrentThread());
  DCHECK(it_ != map_->local_stream_adapters_.end());
  DCHECK(entry()->adapter);
  ++entry()->ref_count;
}

WebRtcMediaStreamAdapterMap::AdapterRef::~AdapterRef() {
  DCHECK(map_->main_thread_->BelongsToCurrentThread());
  if (--entry()->ref_count == 0) {
    map_->local_stream_adapters_.erase(it_);
  }
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::AdapterRef::Copy() const {
  return base::WrapUnique(new AdapterRef(map_, it_));
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
  DCHECK(main_thread_->BelongsToCurrentThread());
  auto it = local_stream_adapters_.find(id);
  if (it == local_stream_adapters_.end())
    return nullptr;
  return base::WrapUnique(new AdapterRef(this, it));
}

std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>
WebRtcMediaStreamAdapterMap::GetOrCreateLocalStreamAdapter(
    const blink::WebMediaStream& web_stream) {
  DCHECK(main_thread_->BelongsToCurrentThread());
  std::string id = web_stream.Id().Utf8();
  auto it = local_stream_adapters_.find(id);
  if (it == local_stream_adapters_.end()) {
    it = local_stream_adapters_
             .insert(std::make_pair(
                 id, AdapterEntry(base::MakeUnique<WebRtcMediaStreamAdapter>(
                         factory_, track_adapter_map_, web_stream))))
             .first;
  }
  return base::WrapUnique(new AdapterRef(this, it));
}

size_t WebRtcMediaStreamAdapterMap::GetLocalStreamCount() const {
  DCHECK(main_thread_->BelongsToCurrentThread());
  return local_stream_adapters_.size();
}

}  // namespace content
