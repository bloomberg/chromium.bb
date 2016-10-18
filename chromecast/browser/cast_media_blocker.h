// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
#define CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_

#include "base/macros.h"
#include "base/callback_forward.h"
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

  // Constructor for test. |suspend_cb| and |resume_cb| will be called when
  // Suspend() and Resume() are called.
  CastMediaBlocker(content::WebContents* web_contents,
                   const base::Closure& suspend_cb,
                   const base::Closure& resume_cb);

  ~CastMediaBlocker() override;

  // Sets if the web contents is allowed to play media or not. If media is
  // unblocked, previously suspended elements should begin playing again.
  void BlockMediaLoading(bool blocked);

  // content::WebContentsObserver implementation:
  void MediaSessionStateChanged(bool is_controllable,
                                bool is_suspended) override;

 protected:
  bool media_loading_blocked() const { return blocked_; }

  // Blocks or unblocks the render process from loading new media
  // according to |media_loading_blocked_|.
  virtual void UpdateMediaBlockedState() {}

 private:
  // Suspends or resumes the media session for the web contents.
  void Suspend();
  void Resume();

  base::Closure suspend_cb_;
  base::Closure resume_cb_;

  // Whether or not media should be blocked. This value cache's the last call to
  // BlockMediaLoading. Is false by default.
  bool blocked_;

  // Whether or not the user paused media on the page.
  bool paused_by_user_;

  // Whether or not media in the app can be controlled and if media is currently
  // suspended. These variables cache arguments from MediaSessionStateChanged().
  bool suspended_;
  bool controllable_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaBlocker);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_MEDIA_BLOCKER_H_
