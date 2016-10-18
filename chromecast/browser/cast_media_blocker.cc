// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_media_blocker.h"

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {
namespace shell {

CastMediaBlocker::CastMediaBlocker(content::WebContents* web_contents)
    : CastMediaBlocker::CastMediaBlocker(
          web_contents,
          base::Bind(&CastMediaBlocker::Suspend, base::Unretained(this)),
          base::Bind(&CastMediaBlocker::Resume, base::Unretained(this))) {}

CastMediaBlocker::CastMediaBlocker(content::WebContents* web_contents,
                                   const base::Closure& suspend_cb,
                                   const base::Closure& resume_cb)
    : content::WebContentsObserver(web_contents),
      suspend_cb_(suspend_cb),
      resume_cb_(resume_cb),
      blocked_(false),
      paused_by_user_(true),
      suspended_(true),
      controllable_(false) {}

CastMediaBlocker::~CastMediaBlocker() {}

void CastMediaBlocker::BlockMediaLoading(bool blocked) {
  if (blocked_ == blocked)
    return;

  blocked_ = blocked;
  UpdateMediaBlockedState();

  LOG(INFO) << __FUNCTION__ << " blocked=" << blocked_
            << " suspended=" << suspended_ << " controllable=" << controllable_
            << " paused_by_user=" << paused_by_user_;

  // If blocking media, suspend if possible.
  if (blocked_) {
    if (!suspended_ && controllable_) {
      suspend_cb_.Run();
    }
    return;
  }

  // If unblocking media, resume if media was not paused by user.
  if (!paused_by_user_ && suspended_ && controllable_) {
    paused_by_user_ = true;
    resume_cb_.Run();
  }
}

void CastMediaBlocker::MediaSessionStateChanged(bool is_controllable,
                                                bool is_suspended) {
  LOG(INFO) << __FUNCTION__ << " blocked=" << blocked_
            << " is_suspended=" << is_suspended
            << " is_controllable=" << is_controllable
            << " paused_by_user=" << paused_by_user_;

  // Process controllability first.
  if (controllable_ != is_controllable) {
    controllable_ = is_controllable;

    // If not blocked, and we regain control and the media wasn't paused when
    // blocked, resume media if suspended.
    if (!blocked_ && !paused_by_user_ && is_suspended && controllable_) {
      paused_by_user_ = true;
      resume_cb_.Run();
    }

    // Suspend if blocked and the session becomes controllable.
    if (blocked_ && !is_suspended && controllable_) {
      // Only suspend if suspended_ doesn't change. Otherwise, this will be
      // handled in the suspended changed block.
      if (suspended_ == is_suspended)
        suspend_cb_.Run();
    }
  }

  // Process suspended state next.
  if (suspended_ != is_suspended) {
    suspended_ = is_suspended;
    // If blocking, suspend media whenever possible.
    if (blocked_ && !suspended_) {
      // If media was resumed when blocked, the user tried to play music.
      paused_by_user_ = false;
      if (controllable_)
        suspend_cb_.Run();
    }

    // If not blocking, cache the user's play intent.
    if (!blocked_)
      paused_by_user_ = suspended_;
  }
}

void CastMediaBlocker::Suspend() {
  if (!web_contents())
    return;

  LOG(INFO) << "Suspending media session.";
  web_contents()->SuspendMediaSession();
}

void CastMediaBlocker::Resume() {
  if (!web_contents())
    return;

  LOG(INFO) << "Resuming media session.";
  web_contents()->ResumeMediaSession();
}

}  // namespace shell
}  // namespace chromecast
