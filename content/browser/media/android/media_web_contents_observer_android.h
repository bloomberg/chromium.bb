// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_WEB_CONTENTS_OBSERVER_ANDROID_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_WEB_CONTENTS_OBSERVER_ANDROID_H_

#include <stdint.h>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/common/content_export.h"

namespace content {

class BrowserCdmManager;
class BrowserMediaPlayerManager;
class BrowserMediaSessionManager;
class MediaSessionController;

// This class adds Android specific extensions to the MediaWebContentsObserver.
class CONTENT_EXPORT MediaWebContentsObserverAndroid
    : public MediaWebContentsObserver {
 public:
  explicit MediaWebContentsObserverAndroid(WebContents* web_contents);
  ~MediaWebContentsObserverAndroid() override;

  // Returns the android specific observer for a given web contents.
  static MediaWebContentsObserverAndroid* FromWebContents(
      WebContents* web_contents);

  // Gets the media player or media session manager associated with the given
  // |render_frame_host| respectively. Creates a new one if it doesn't exist.
  // The caller doesn't own the returned pointer.
  BrowserMediaPlayerManager* GetMediaPlayerManager(
      RenderFrameHost* render_frame_host);
  BrowserMediaSessionManager* GetMediaSessionManager(
      RenderFrameHost* render_frame_host);

  // Called by the WebContents when a tab has been closed but may still be
  // available for "undo" -- indicates that all media players (even audio only
  // players typically allowed background audio) bound to this WebContents must
  // be suspended.
  void SuspendAllMediaPlayers();

  // Initiates a synchronous MediaSession request for browser side players.
  //
  // TODO(dalecurtis): Delete this method once we're no longer using WMPA and
  // the BrowserMediaPlayerManagers.  Tracked by http://crbug.com/580626
  bool RequestPlay(RenderFrameHost* render_frame_host,
                   int delegate_id,
                   bool has_audio,
                   bool is_remote,
                   base::TimeDelta duration);

#if defined(VIDEO_HOLE)
  void OnFrameInfoUpdated();
#endif  // defined(VIDEO_HOLE)

  // MediaWebContentsObserverAndroid overrides.
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

 private:
  // Handles messages from the WebMediaPlayerDelegate; does not modify the
  // handled state since the superclass needs to handle these as well.
  void OnMediaPlayerDelegateMessageReceived(const IPC::Message& msg,
                                            RenderFrameHost* render_frame_host);
  void OnMediaDestroyed(RenderFrameHost* render_frame_host, int delegate_id);
  void OnMediaPaused(RenderFrameHost* render_frame_host,
                     int delegate_id,
                     bool reached_end_of_stream);
  void OnMediaPlaying(RenderFrameHost* render_frame_host,
                      int delegate_id,
                      bool has_video,
                      bool has_audio,
                      bool is_remote,
                      base::TimeDelta duration);

  // Helper functions to handle media player IPC messages. Returns whether the
  // |message| is handled in the function.
  bool OnMediaPlayerMessageReceived(const IPC::Message& message,
                                    RenderFrameHost* render_frame_host);

  bool OnMediaPlayerSetCdmMessageReceived(const IPC::Message& message,
                                          RenderFrameHost* render_frame_host);

  bool OnMediaSessionMessageReceived(const IPC::Message& message,
                                     RenderFrameHost* render_frame_host);

  void OnSetCdm(RenderFrameHost* render_frame_host, int player_id, int cdm_id);

  // Map from RenderFrameHost* to BrowserMediaPlayerManager.
  using MediaPlayerManagerMap =
      base::ScopedPtrHashMap<RenderFrameHost*,
                             scoped_ptr<BrowserMediaPlayerManager>>;
  MediaPlayerManagerMap media_player_managers_;

  // Map from RenderFrameHost* to BrowserMediaSessionManager.
  using MediaSessionManagerMap =
      base::ScopedPtrHashMap<RenderFrameHost*,
                             scoped_ptr<BrowserMediaSessionManager>>;
  MediaSessionManagerMap media_session_managers_;

  // Map of renderer process media players to session controllers.
  using MediaSessionMap =
      std::map<MediaPlayerId, scoped_ptr<MediaSessionController>>;
  MediaSessionMap media_session_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaWebContentsObserverAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_ANDROID_MEDIA_WEB_CONTENTS_OBSERVER_ANDROID_H_
