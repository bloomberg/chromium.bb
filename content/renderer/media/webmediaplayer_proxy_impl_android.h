// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PROXY_IMPL_ANDROID_H_
#define CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PROXY_IMPL_ANDROID_H_

#include <map>

#include "content/public/renderer/render_view_observer.h"
#include "webkit/media/android/webmediaplayer_proxy_android.h"

namespace webkit_media {
class WebMediaPlayerAndroid;
class WebMediaPlayerManagerAndroid;
}

namespace content {

// This class manages all the IPC communications between
// WebMediaPlayerAndroid and the MediaPlayerManagerAndroid in the browser
// process.
class WebMediaPlayerProxyImplAndroid
    : public RenderViewObserver,
      public webkit_media::WebMediaPlayerProxyAndroid {
 public:
  // Construct a WebMediaPlayerProxyImplAndroid object for the |render_view|.
  // |manager| is passed to this class so that it can find the right
  // WebMediaPlayerAndroid using player IDs.
  WebMediaPlayerProxyImplAndroid(
      RenderView* render_view,
      webkit_media::WebMediaPlayerManagerAndroid* manager);
  virtual ~WebMediaPlayerProxyImplAndroid();

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Methods inherited from WebMediaPlayerProxyAndroid.
  virtual void Initialize(int player_id, const GURL& url,
                          bool is_media_source,
                          const GURL& first_party_for_cookies) OVERRIDE;
  virtual void Start(int player_id) OVERRIDE;
  virtual void Pause(int player_id) OVERRIDE;
  virtual void Seek(int player_id, base::TimeDelta time) OVERRIDE;
  virtual void ReleaseResources(int player_id) OVERRIDE;
  virtual void DestroyPlayer(int player_id) OVERRIDE;
  virtual void EnterFullscreen(int player_id) OVERRIDE;
  virtual void ExitFullscreen(int player_id) OVERRIDE;
#if defined(GOOGLE_TV)
  virtual void RequestExternalSurface(int player_id) OVERRIDE;
  virtual void DemuxerReady(
      int player_id,
      const media::MediaPlayerHostMsg_DemuxerReady_Params& params) OVERRIDE;
  virtual void ReadFromDemuxerAck(
      int player_id,
      const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params&) OVERRIDE;

  // Methods inherited from RenderViewObserver.
  virtual void DidCommitCompositorFrame() OVERRIDE;
#endif

 private:
  webkit_media::WebMediaPlayerAndroid* GetWebMediaPlayer(int player_id);

  // Message handlers.
  void OnMediaMetadataChanged(int player_id, base::TimeDelta duration,
                              int width, int height, bool success);
  void OnMediaPlaybackCompleted(int player_id);
  void OnMediaBufferingUpdate(int player_id, int percent);
  void OnMediaSeekCompleted(int player_id, base::TimeDelta current_time);
  void OnMediaError(int player_id, int error);
  void OnVideoSizeChanged(int player_id, int width, int height);
  void OnTimeUpdate(int player_id, base::TimeDelta current_time);
  void OnMediaPlayerReleased(int player_id);
  void OnDidExitFullscreen(int player_id);
  void OnDidEnterFullscreen(int player_id);
  void OnPlayerPlay(int player_id);
  void OnPlayerPause(int player_id);
#if defined(GOOGLE_TV)
  void OnReadFromDemuxer(
      int player_id, media::DemuxerStream::Type type, bool seek_done);
#endif

  webkit_media::WebMediaPlayerManagerAndroid* manager_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerProxyImplAndroid);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBMEDIAPLAYER_PROXY_IMPL_ANDROID_H_
