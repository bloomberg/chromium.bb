// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_MEDIA_PLAYER_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ANDROID_MEDIA_PLAYER_MANAGER_IMPL_H_

#include <map>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "content/browser/android/content_video_view.h"
#include "content/public/browser/render_view_host_observer.h"
#include "googleurl/src/gurl.h"
#if defined(GOOGLE_TV)
#include "media/base/android/demuxer_stream_player_params.h"
#endif
#include "media/base/android/media_player_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "ui/gfx/rect_f.h"

namespace content {

class WebContents;

// This class manages all the MediaPlayerBridge objects. It receives
// control operations from the the render process, and forwards
// them to corresponding MediaPlayerBridge object. Callbacks from
// MediaPlayerBridge objects are converted to IPCs and then sent to the
// render process.
class MediaPlayerManagerImpl
    : public RenderViewHostObserver,
      public media::MediaPlayerManager {
 public:
  // Create a MediaPlayerManagerImpl object for the |render_view_host|.
  explicit MediaPlayerManagerImpl(RenderViewHost* render_view_host);
  virtual ~MediaPlayerManagerImpl();

  // RenderViewHostObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Fullscreen video playback controls.
  void FullscreenPlayerPlay();
  void FullscreenPlayerPause();
  void FullscreenPlayerSeek(int msec);
  void ExitFullscreen(bool release_media_player);
  void SetVideoSurface(jobject surface);

  // An internal method that checks for current time routinely and generates
  // time update events.
  void OnTimeUpdate(int player_id, base::TimeDelta current_time);

  // Callbacks needed by media::MediaPlayerBridge.
  void OnMediaMetadataChanged(int player_id, base::TimeDelta duration,
                              int width, int height, bool success);
  void OnPlaybackComplete(int player_id);
  void OnMediaInterrupted(int player_id);
  void OnBufferingUpdate(int player_id, int percentage);
  void OnSeekComplete(int player_id, base::TimeDelta current_time);
  void OnError(int player_id, int error);
  void OnVideoSizeChanged(int player_id, int width, int height);

#if defined(GOOGLE_TV)
  // Callbacks needed by media::DemuxerStreamPlayer.
  void OnReadFromDemuxer(
      int player_id, media::DemuxerStream::Type type, bool seek_done);
#endif

  // media::MediaPlayerBridgeManager overrides.
  virtual void RequestMediaResources(media::MediaPlayerBridge* player) OVERRIDE;
  virtual void ReleaseMediaResources(media::MediaPlayerBridge* player) OVERRIDE;
  virtual media::MediaResourceGetter* GetMediaResourceGetter() OVERRIDE;

  // Release all the players managed by this object.
  void DestroyAllMediaPlayers();

#if defined(GOOGLE_TV)
  void AttachExternalVideoSurface(int player_id, jobject surface);
  void DetachExternalVideoSurface(int player_id);
#endif

  media::MediaPlayerBridge* GetFullscreenPlayer();
  media::MediaPlayerBridge* GetPlayer(int player_id);

 private:
  // Message handlers.
  void OnEnterFullscreen(int player_id);
  void OnExitFullscreen(int player_id);
  void OnInitialize(int player_id, const GURL& url,
                    bool is_media_source,
                    const GURL& first_party_for_cookies);
  void OnStart(int player_id);
  void OnSeek(int player_id, base::TimeDelta time);
  void OnPause(int player_id);
  void OnReleaseResources(int player_id);
  void OnDestroyPlayer(int player_id);
#if defined(GOOGLE_TV)
  void OnRequestExternalSurface(int player_id);
  void OnNotifyGeometryChange(int player_id, const gfx::RectF& rect);
  void OnDemuxerReady(
      int player_id,
      const media::MediaPlayerHostMsg_DemuxerReady_Params& params);
  void OnReadFromDemuxerAck(
      int player_id,
      const media::MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params);
#endif

  // An array of managed players.
  ScopedVector<media::MediaPlayerBridge> players_;

  // The fullscreen video view object.
  ContentVideoView video_view_;

  // Player ID of the fullscreen media player.
  int fullscreen_player_id_;

  WebContents* web_contents_;

  // Object for retrieving resources media players.
  scoped_ptr<media::MediaResourceGetter> media_resource_getter_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_MEDIA_PLAYER_MANAGER_IMPL_H_
