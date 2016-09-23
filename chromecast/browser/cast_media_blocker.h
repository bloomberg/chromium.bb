// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
#define CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

// This class implements a blocking mode for web applications and is used in
// Chromecast internal code. Media is unblocked by default.
class CastMediaBlocker : public content::WebContentsObserver {
 public:
  explicit CastMediaBlocker(content::WebContents* web_contents);
  ~CastMediaBlocker() override;

  // Sets if the web contents is allowed to play media or not. If media is
  // unblocked, previously suspended elements should begin playing again.
  void BlockMediaLoading(bool blocked);

 protected:
  bool media_loading_blocked() const { return state_ == BLOCKED; }

  // Blocks or unblocks the render process from loading new media
  // according to |media_loading_blocked_|.
  virtual void UpdateMediaBlockedState();

 private:
  enum BlockState {
    BLOCKED,     // All media playback should be blocked.
    UNBLOCKING,  // Media playback should be resumed at next oppurtunity.
    UNBLOCKED,   // Media playback should not be blocked.
  };

  // content::WebContentsObserver implementation:
  void MediaSessionStateChanged(
      bool is_controllable,
      bool is_suspended,
      const base::Optional<content::MediaMetadata>& metadata) override;

  // Whether or not media in the app can be controlled and if media is currently
  // suspended. These variables cache arguments from MediaSessionStateChanged().
  bool controllable_;
  bool suspended_;

  BlockState state_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaBlocker);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
