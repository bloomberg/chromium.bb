// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#import <objc/runtime.h>

#import "ios/web_view/internal/signin/cwv_authentication_controller_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVWebViewConfiguration (Sync)

- (CWVAuthenticationController*)authenticationController {
  CWVAuthenticationController* authenticationController =
      objc_getAssociatedObject(self, @selector(authenticationController));
  ios_web_view::WebViewBrowserState* browserState = self.browserState;
  if (!authenticationController && !browserState->IsOffTheRecord()) {
    authenticationController =
        [[CWVAuthenticationController alloc] initWithBrowserState:browserState];
    objc_setAssociatedObject(self, @selector(authenticationController),
                             authenticationController,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  }
  return authenticationController;
}

@end
