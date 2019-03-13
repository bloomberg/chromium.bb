// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/app_url_loading_service.h"

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AppUrlLoadingService::AppUrlLoadingService() {}

void AppUrlLoadingService::SetDelegate(
    id<AppURLLoadingServiceDelegate> delegate) {
  delegate_ = delegate;
}

void AppUrlLoadingService::LoadUrlInNewTab(UrlLoadParams* params) {
  DCHECK(delegate_);
  // TODO(crbug.com/907527): won't need to create command, once code in MC
  // (delegate) is moved here.
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:params->web_params.url
                                    referrer:params->web_params.referrer
                                 inIncognito:params->in_incognito
                                inBackground:params->in_background()
                                    appendTo:params->append_to];
  [delegate_ openURLInNewTabWithCommand:command];
}
