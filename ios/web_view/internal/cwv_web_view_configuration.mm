// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#include <memory>

#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_preferences_internal.h"
#import "ios/web_view/internal/cwv_user_content_controller_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/web_view_global_state_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVWebViewConfiguration () {
  // The BrowserState for this configuration.
  std::unique_ptr<ios_web_view::WebViewBrowserState> _browserState;

  // Holds all CWVWebViews created with this class. Weak references.
  NSHashTable* _webViews;

  // |YES| if |shutDown| was called.
  BOOL _wasShutDown;
}

// Initializes configuration with the specified browser state mode.
- (instancetype)initWithBrowserState:
    (std::unique_ptr<ios_web_view::WebViewBrowserState>)browserState;
@end

@implementation CWVWebViewConfiguration

@synthesize preferences = _preferences;
@synthesize userContentController = _userContentController;

namespace {
CWVWebViewConfiguration* gDefaultConfiguration = nil;
CWVWebViewConfiguration* gIncognitoConfiguration = nil;
}  // namespace

+ (void)shutDown {
  [gDefaultConfiguration shutDown];
  [gIncognitoConfiguration shutDown];
}

+ (instancetype)defaultConfiguration {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    auto browserState =
        std::make_unique<ios_web_view::WebViewBrowserState>(false);
    gDefaultConfiguration = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::move(browserState)];
  });
  return gDefaultConfiguration;
}

+ (instancetype)incognitoConfiguration {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    auto browserState =
        std::make_unique<ios_web_view::WebViewBrowserState>(true);
    gIncognitoConfiguration = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::move(browserState)];
  });
  return gIncognitoConfiguration;
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

    _webViews = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

- (void)dealloc {
  DCHECK(_wasShutDown);
}

#pragma mark - Public Methods

- (BOOL)isPersistent {
  return !_browserState->IsOffTheRecord();
}

#pragma mark - Private Methods

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState.get();
}

- (void)registerWebView:(CWVWebView*)webView {
  [_webViews addObject:webView];
}

- (void)shutDown {
  for (CWVWebView* webView in _webViews) {
    [webView shutDown];
  }
  _browserState.reset();
  _wasShutDown = YES;
}

@end
