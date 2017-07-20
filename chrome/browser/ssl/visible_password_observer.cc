// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/visible_password_observer.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/common/origin_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(VisiblePasswordObserver);

VisiblePasswordObserver::~VisiblePasswordObserver() {}

void VisiblePasswordObserver::RenderFrameHasVisiblePasswordField(
    content::RenderFrameHost* render_frame_host) {
  frames_with_visible_password_fields_.insert(render_frame_host);
  MaybeNotifyPasswordInputShownOnHttp();
}

void VisiblePasswordObserver::RenderFrameHasNoVisiblePasswordFields(
    content::RenderFrameHost* render_frame_host) {
  frames_with_visible_password_fields_.erase(render_frame_host);
  MaybeNotifyAllFieldsInvisible();
}

void VisiblePasswordObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // After a renderer crashes, it has no visible password fields.
  RenderFrameHasNoVisiblePasswordFields(render_frame_host);
}

VisiblePasswordObserver::VisiblePasswordObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), web_contents_(web_contents) {}

void VisiblePasswordObserver::MaybeNotifyPasswordInputShownOnHttp() {
  web_contents_->OnPasswordInputShownOnHttp();
}

void VisiblePasswordObserver::MaybeNotifyAllFieldsInvisible() {
  if (frames_with_visible_password_fields_.empty()) {
    web_contents_->OnAllPasswordInputsHiddenOnHttp();
  }
}
