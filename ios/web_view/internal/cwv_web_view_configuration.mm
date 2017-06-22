// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_user_content_controller_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/web_view_global_state_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVWebViewConfiguration () {
  // The BrowserState for this configuration.
  std::unique_ptr<ios_web_view::WebViewBrowserState> _browserState;
}

// Initializes configuration with the specified browser state mode.
- (instancetype)initWithBrowserState:
    (std::unique_ptr<ios_web_view::WebViewBrowserState>)browserState;
@end

@implementation CWVWebViewConfiguration

@synthesize userContentController = _userContentController;

+ (instancetype)defaultConfiguration {
  auto browserState =
      base::MakeUnique<ios_web_view::WebViewBrowserState>(false);
  return [[self alloc] initWithBrowserState:std::move(browserState)];
}

+ (instancetype)incognitoConfiguration {
  auto browserState = base::MakeUnique<ios_web_view::WebViewBrowserState>(true);
  return [[self alloc] initWithBrowserState:std::move(browserState)];
}

+ (void)initialize {
  if (self != [CWVWebViewConfiguration class]) {
    return;
  }

  ios_web_view::InitializeGlobalState();
}

- (instancetype)initWithBrowserState:
    (std::unique_ptr<ios_web_view::WebViewBrowserState>)browserState {
  self = [super init];
  if (self) {
    _browserState = std::move(browserState);

    _userContentController =
        [[CWVUserContentController alloc] initWithConfiguration:self];
  }
  return self;
}

- (BOOL)isPersistent {
  return !_browserState->IsOffTheRecord();
}

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState.get();
}

@end
