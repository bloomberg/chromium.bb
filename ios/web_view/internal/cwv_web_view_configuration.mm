// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_web_view_configuration.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#import "ios/web_view/internal/cwv_user_content_controller_internal.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVWebViewConfiguration ()
// Initialize configuration with the specified browser state.
- (instancetype)initWithBrowserState:
    (ios_web_view::WebViewBrowserState*)browserState;
@end

@implementation CWVWebViewConfiguration {
  // TODO(crbug.com/690182): CWVWebViewConfiguration should own _browserState.
  ios_web_view::WebViewBrowserState* _browserState;
}

@synthesize userContentController = _userContentController;

+ (instancetype)defaultConfiguration {
  static dispatch_once_t once;
  static CWVWebViewConfiguration* configuration;
  dispatch_once(&once, ^{
    ios_web_view::WebViewWebClient* client =
        static_cast<ios_web_view::WebViewWebClient*>(web::GetWebClient());
    configuration = [[self alloc] initWithBrowserState:client->browser_state()];
  });
  return configuration;
}

+ (instancetype)incognitoConfiguration {
  static dispatch_once_t once;
  static CWVWebViewConfiguration* configuration;
  dispatch_once(&once, ^{
    ios_web_view::WebViewWebClient* client =
        static_cast<ios_web_view::WebViewWebClient*>(web::GetWebClient());
    configuration = [[self alloc]
        initWithBrowserState:client->off_the_record_browser_state()];
  });
  return configuration;
}

- (instancetype)initWithBrowserState:
    (ios_web_view::WebViewBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _userContentController =
        [[CWVUserContentController alloc] initWithConfiguration:self];
  }
  return self;
}

- (BOOL)isPersistent {
  return !_browserState->IsOffTheRecord();
}

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState;
}

// NSCopying

- (id)copyWithZone:(NSZone*)zone {
  return [[[self class] allocWithZone:zone] initWithBrowserState:_browserState];
}

@end
