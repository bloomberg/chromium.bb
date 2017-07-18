// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_MAP_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_MAP_H_

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

// A map and owner of |WebRtcMediaStreamAdapter|s. Adapters are the glue between
// blink and webrtc layer versions of streams. As long as a stream is in use by
// a peer connection there has to exist an adapter for it. The map takes care of
// creating and disposing stream adapters. Adapters are accessed via
// |AdapterRef|s, when all references to an adapter are destroyed it is
// destroyed and removed from the map.
class CONTENT_EXPORT WebRtcMediaStreamAdapterMap
    : public base::RefCountedThreadSafe<WebRtcMediaStreamAdapterMap> {
 private:
  // The map's entries are reference counted in order to remove the adapter when
  // all |AdapterRef|s referencing an entry are destroyed.
  // Private section needed here due to |AdapterRef|'s usage of |AdapterEntry|.
  struct AdapterEntry {
    AdapterEntry(std::unique_ptr<WebRtcMediaStreamAdapter> adapter);
    AdapterEntry(AdapterEntry&& other);
    ~AdapterEntry();

    AdapterEntry(const AdapterEntry&) = delete;
    AdapterEntry& operator=(const AdapterEntry&) = delete;

    std::unique_ptr<WebRtcMediaStreamAdapter> adapter;
    size_t ref_count;
  };

 public:
  // Accessor to an adapter to take care of reference counting. When the last
  // |AdapterRef| is destroyed, the corresponding adapter is destroyed and
  // removed from the map.
  class CONTENT_EXPORT AdapterRef {
   public:
    // Must be invoked on the main thread. If this was the last reference to the
    // adapter it will be disposed and removed from the map.
    ~AdapterRef();

    std::unique_ptr<AdapterRef> Copy() const;
    const WebRtcMediaStreamAdapter& adapter() const {
      return *it_->second.adapter;
    }

   private:
    friend class WebRtcMediaStreamAdapterMap;
    using MapEntryIterator = std::map<std::string, AdapterEntry>::iterator;

    // Increments the |AdapterEntry::ref_count|.
    AdapterRef(scoped_refptr<WebRtcMediaStreamAdapterMap> map,
               const MapEntryIterator& it);

    AdapterEntry* entry() { return &it_->second; }

    scoped_refptr<WebRtcMediaStreamAdapterMap> map_;
    MapEntryIterator it_;
  };

  // Must be invoked on the main thread.
  WebRtcMediaStreamAdapterMap(
      PeerConnectionDependencyFactory* const factory,
      scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map);

  // Invoke on the main thread. Gets a new reference to the local stream adapter
  // by ID, or null if no such adapter was found. When all references are
  // destroyed the adapter is destroyed and removed from the map. References
  // must be destroyed on the main thread.
  std::unique_ptr<AdapterRef> GetLocalStreamAdapter(const std::string& id);
  // Invoke on the main thread. Gets a new reference to the local stream adapter
  // for the web stream. If no adapter exists for the stream one is created.
  // When all references are destroyed the adapter is destroyed and removed from
  // the map. References must be destroyed on the main thread.
  std::unique_ptr<AdapterRef> GetOrCreateLocalStreamAdapter(
      const blink::WebMediaStream& web_stream);
  // Invoke on the main thread.
  size_t GetLocalStreamCount() const;

 protected:
  friend class base::RefCountedThreadSafe<WebRtcMediaStreamAdapterMap>;

  // Invoke on the main thread.
  virtual ~WebRtcMediaStreamAdapterMap();

 private:
  // Pointer to a |PeerConnectionDependencyFactory| owned by the |RenderThread|.
  // It's valid for the lifetime of |RenderThread|.
  PeerConnectionDependencyFactory* const factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // Takes care of creating and owning track adapters, used by stream adapters.
  scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;

  std::map<std::string, AdapterEntry> local_stream_adapters_;
  // TODO(hbos): Take care of remote stream adapters as well. This will require
  // usage of the signaling thread. crbug.com/705901
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_ADAPTER_MAP_H_
