// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_TRACK_ADAPTER_MAP_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_TRACK_ADAPTER_MAP_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/two_keys_adapter_map.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

class PeerConnectionDependencyFactory;

// A map and owner of |WebRtcMediaStreamTrackAdapter|s. It takes care of
// creating, initializing and disposing track adapters independently of media
// streams. Adapters are accessed via |AdapterRef|s, when all references to an
// adapter are destroyed it is disposed and removed from the map.
class CONTENT_EXPORT WebRtcMediaStreamTrackAdapterMap
    : public base::RefCountedThreadSafe<WebRtcMediaStreamTrackAdapterMap> {
 public:
  // Acts as an accessor to adapter members without leaking a reference to the
  // adapter. When the last |AdapterRef| is destroyed, the corresponding adapter
  // is |Dispose|d and removed from the map.
  class CONTENT_EXPORT AdapterRef {
   public:
    // Must be invoked on the main thread. If this was the last reference to the
    // adapter it will be disposed and removed from the map.
    ~AdapterRef();

    std::unique_ptr<AdapterRef> Copy() const;
    bool is_initialized() const { return adapter_->is_initialized(); }
    const blink::WebMediaStreamTrack& web_track() const {
      return adapter_->web_track();
    }
    webrtc::MediaStreamTrackInterface* webrtc_track() const {
      return adapter_->webrtc_track();
    }

    // Warning: Holding an external reference to the adapter will prevent
    // |~AdapterRef| from disposing the adapter.
    WebRtcMediaStreamTrackAdapter* GetAdapterForTesting() const {
      return adapter_.get();
    }

   private:
    friend class WebRtcMediaStreamTrackAdapterMap;

    enum class Type { kLocal, kRemote };

    // Assumes map's |lock_| is held.
    AdapterRef(scoped_refptr<WebRtcMediaStreamTrackAdapterMap> map,
               Type type,
               scoped_refptr<WebRtcMediaStreamTrackAdapter> adapter);

    scoped_refptr<WebRtcMediaStreamTrackAdapterMap> map_;
    Type type_;
    // A reference to the entry's adapter, ensures that |HasOneRef()| is false
    // as long as the |AdapterRef| is kept alive (the map entry has one
    // reference to it too).
    scoped_refptr<WebRtcMediaStreamTrackAdapter> adapter_;
  };

  // Must be invoked on the main thread.
  WebRtcMediaStreamTrackAdapterMap(
      PeerConnectionDependencyFactory* const factory);

  // Gets a new reference to the local track adapter, or null if no such adapter
  // was found. When all references are destroyed the adapter is disposed and
  // removed from the map. This method can be called from any thread, but
  // references must be destroyed on the main thread.
  // The adapter is a associated with a blink and webrtc track, lookup works by
  // either track.
  std::unique_ptr<AdapterRef> GetLocalTrackAdapter(
      const blink::WebMediaStreamTrack& web_track);
  std::unique_ptr<AdapterRef> GetLocalTrackAdapter(
      webrtc::MediaStreamTrackInterface* webrtc_track);
  // Invoke on the main thread. Gets a new reference to the local track adapter
  // for the web track. If no adapter exists for the track one is created and
  // initialized. When all references are destroyed the adapter is disposed and
  // removed from the map. References must be destroyed on the main thread.
  std::unique_ptr<AdapterRef> GetOrCreateLocalTrackAdapter(
      const blink::WebMediaStreamTrack& web_track);
  size_t GetLocalTrackCount() const;

  // Gets a new reference to the remote track adapter if it exists and is
  // initialized, null otherwise. When all references are destroyed the adapter
  // is disposed and removed from the map. This method can be called from any
  // thread, but references must be destroyed on the main thread.
  // The adapter is a associated with a blink and webrtc track, lookup works by
  // either track.
  std::unique_ptr<AdapterRef> GetRemoteTrackAdapter(
      const blink::WebMediaStreamTrack& web_track);
  std::unique_ptr<AdapterRef> GetRemoteTrackAdapter(
      webrtc::MediaStreamTrackInterface* webrtc_track);
  // Invoke on the webrtc signaling thread. Gets a new reference to the remote
  // track adapter for the webrtc track. If no adapter exists for the track one
  // is created and initialization completes on the main thread in a post. When
  // all references are destroyed the adapter is disposed and removed from the
  // map. References must be destroyed on the main thread.
  std::unique_ptr<AdapterRef> GetOrCreateRemoteTrackAdapter(
      webrtc::MediaStreamTrackInterface* webrtc_track);
  size_t GetRemoteTrackCount() const;

 protected:
  friend class base::RefCountedThreadSafe<WebRtcMediaStreamTrackAdapterMap>;

  // Invoke on the main thread.
  virtual ~WebRtcMediaStreamTrackAdapterMap();

 private:
  using LocalTrackAdapterMap =
      TwoKeysAdapterMap<int,
                        webrtc::MediaStreamTrackInterface*,
                        scoped_refptr<WebRtcMediaStreamTrackAdapter>>;
  using RemoteTrackAdapterMap =
      TwoKeysAdapterMap<webrtc::MediaStreamTrackInterface*,
                        int,
                        scoped_refptr<WebRtcMediaStreamTrackAdapter>>;

  void OnRemoteTrackAdapterInitialized(std::unique_ptr<AdapterRef> adapter_ref);

  // Pointer to a |PeerConnectionDependencyFactory| owned by the |RenderThread|.
  // It's valid for the lifetime of |RenderThread|.
  PeerConnectionDependencyFactory* const factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;

  mutable base::Lock lock_;
  LocalTrackAdapterMap local_track_adapters_;
  RemoteTrackAdapterMap remote_track_adapters_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_MEDIA_STREAM_TRACK_ADAPTER_MAP_H_
