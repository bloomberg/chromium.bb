// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/criwv.h"

#include <memory>

#include "base/location.h"
#import "base/mac/bind_objc_block.h"
#include "base/single_thread_task_runner.h"
#include "ios/web/public/app/web_main.h"
#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/criwv_browser_state.h"
#import "ios/web_view/internal/criwv_web_client.h"
#import "ios/web_view/internal/criwv_web_main_delegate.h"
#import "ios/web_view/internal/criwv_web_view_internal.h"
#import "ios/web_view/public/criwv_delegate.h"

namespace {
CRIWV* g_criwv = nil;
}

@interface CRIWV () {
  id<CRIWVDelegate> _delegate;
  std::unique_ptr<ios_web_view::CRIWVWebMainDelegate> _webMainDelegate;
  std::unique_ptr<web::WebMain> _webMain;
}

- (instancetype)initWithDelegate:(id<CRIWVDelegate>)delegate;
- (ios_web_view::CRIWVBrowserState*)browserState;
@end

@implementation CRIWV

+ (void)configureWithDelegate:(id<CRIWVDelegate>)delegate {
  g_criwv = [[CRIWV alloc] initWithDelegate:delegate];
}

+ (void)shutDown {
  [g_criwv release];
  g_criwv = nil;
}

+ (CRIWVWebView*)webViewWithFrame:(CGRect)frame {
  return
      [[[CRIWVWebView alloc] initWithFrame:frame
                              browserState:[g_criwv browserState]] autorelease];
}

- (instancetype)initWithDelegate:(id<CRIWVDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _webMainDelegate.reset(new ios_web_view::CRIWVWebMainDelegate(_delegate));
    web::WebMainParams params(_webMainDelegate.get());
    _webMain.reset(new web::WebMain(params));
  }
  return self;
}

- (void)dealloc {
  _webMain.reset();
  _webMainDelegate.reset();
  [super dealloc];
}

- (ios_web_view::CRIWVBrowserState*)browserState {
  ios_web_view::CRIWVWebClient* client =
      static_cast<ios_web_view::CRIWVWebClient*>(web::GetWebClient());
  return client->browser_state();
}

@end
