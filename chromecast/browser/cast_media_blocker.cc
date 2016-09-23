// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_media_blocker.h"

#include "base/threading/thread_checker.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {
namespace shell {

CastMediaBlocker::CastMediaBlocker(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      controllable_(false),
      suspended_(false),
      state_(UNBLOCKED) {}

CastMediaBlocker::~CastMediaBlocker() {}

void CastMediaBlocker::BlockMediaLoading(bool blocked) {
  if (blocked) {
    state_ = BLOCKED;
  } else if (state_ == BLOCKED) {
    state_ = suspended_ ? UNBLOCKING : UNBLOCKED;
  } else {
    return;
  }

  UpdateMediaBlockedState();
}

void CastMediaBlocker::UpdateMediaBlockedState() {
  if (!web_contents()) {
    return;
  }

  if (!controllable_) {
    return;
  }

  if (state_ == BLOCKED && !suspended_) {
    web_contents()->SuspendMediaSession();
  } else if (state_ == UNBLOCKING && suspended_) {
    web_contents()->ResumeMediaSession();
    state_ = UNBLOCKED;
  }
}

void CastMediaBlocker::MediaSessionStateChanged(
    bool is_controllable,
    bool is_suspended,
    const base::Optional<content::MediaMetadata>& metadata) {
  controllable_ = is_controllable;
  suspended_ = is_suspended;
  UpdateMediaBlockedState();
}

}  // namespace shell
}  // namespace chromecast
