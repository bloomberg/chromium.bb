// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_preferences_internal.h"
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

@synthesize preferences = _preferences;
@synthesize userContentController = _userContentController;

+ (instancetype)defaultConfiguration {
  static CWVWebViewConfiguration* defaultConfiguration;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    auto browserState =
        base::MakeUnique<ios_web_view::WebViewBrowserState>(false);
    defaultConfiguration = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::move(browserState)];
  });
  return defaultConfiguration;
}

+ (instancetype)incognitoConfiguration {
  static CWVWebViewConfiguration* incognitoConfiguration;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    auto browserState =
        base::MakeUnique<ios_web_view::WebViewBrowserState>(true);
    incognitoConfiguration = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::move(browserState)];
  });
  return incognitoConfiguration;
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

    _preferences =
        [[CWVPreferences alloc] initWithPrefService:_browserState->GetPrefs()];

    _userContentController =
        [[CWVUserContentController alloc] initWithConfiguration:self];
  }
  return self;
}

#pragma mark - Public Methods

- (BOOL)isPersistent {
  return !_browserState->IsOffTheRecord();
}

#pragma mark - Private Methods

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState.get();
}

@end
