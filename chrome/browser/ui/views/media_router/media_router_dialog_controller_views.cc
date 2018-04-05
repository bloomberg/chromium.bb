// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/media_router_dialog_controller_views.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_webui_impl.h"
#include "chrome/common/chrome_features.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::MediaRouterDialogControllerViews);

namespace media_router {

// static
MediaRouterDialogControllerImplBase*
MediaRouterDialogControllerImplBase::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  if (base::FeatureList::IsEnabled(features::kViewsCastDialog)) {
    return MediaRouterDialogControllerViews::GetOrCreateForWebContents(
        web_contents);
  } else {
    return MediaRouterDialogControllerWebUIImpl::GetOrCreateForWebContents(
        web_contents);
  }
}

MediaRouterDialogControllerViews::~MediaRouterDialogControllerViews() {
  Reset();
}

// static
MediaRouterDialogControllerViews*
MediaRouterDialogControllerViews::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // This call does nothing if the controller already exists.
  MediaRouterDialogControllerViews::CreateForWebContents(web_contents);
  return MediaRouterDialogControllerViews::FromWebContents(web_contents);
}

void MediaRouterDialogControllerViews::CreateMediaRouterDialog() {
  MediaRouterDialogControllerImplBase::CreateMediaRouterDialog();
  // TODO(crbug.com/826091): Implement this method.
}

void MediaRouterDialogControllerViews::CloseMediaRouterDialog() {
  // TODO(crbug.com/826091): Implement this method.
}

bool MediaRouterDialogControllerViews::IsShowingMediaRouterDialog() const {
  // TODO(crbug.com/826091): Implement this method.
  return false;
}

void MediaRouterDialogControllerViews::Reset() {
  MediaRouterDialogControllerImplBase::Reset();
}

MediaRouterDialogControllerViews::MediaRouterDialogControllerViews(
    content::WebContents* web_contents)
    : MediaRouterDialogControllerImplBase(web_contents) {}

}  // namespace media_router
