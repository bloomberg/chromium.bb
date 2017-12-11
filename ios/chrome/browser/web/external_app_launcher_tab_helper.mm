// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/external_app_launcher_tab_helper.h"

#import "ios/chrome/browser/web/external_app_launcher.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(ExternalAppLauncherTabHelper);

ExternalAppLauncherTabHelper::ExternalAppLauncherTabHelper(
    web::WebState* web_state) {
  launcher_ = [[ExternalAppLauncher alloc] init];
}

ExternalAppLauncherTabHelper::~ExternalAppLauncherTabHelper() = default;

BOOL ExternalAppLauncherTabHelper::RequestToOpenUrl(const GURL& url,
                                                    const GURL& source_page_url,
                                                    BOOL link_clicked) {
  return [launcher_ requestToOpenURL:url
                       sourcePageURL:source_page_url
                         linkClicked:link_clicked];
}
