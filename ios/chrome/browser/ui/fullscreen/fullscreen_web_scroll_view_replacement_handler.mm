// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_scroll_view_replacement_handler.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_view_scroll_view_replacement_util.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FullscreenWebScrollViewReplacementHandler ()<
    CRWWebViewScrollViewProxyObserver>
// The model passed on initialization.
@property(nonatomic, assign) FullscreenModel* model;
@end

@implementation FullscreenWebScrollViewReplacementHandler
@synthesize proxy = _proxy;
@synthesize model = _model;

- (instancetype)initWithModel:(FullscreenModel*)model {
  if (self = [super init]) {
    _model = model;
    DCHECK(_model);
  }
  return self;
}

#pragma mark - Accessors

- (void)setProxy:(id<CRWWebViewProxy>)proxy {
  if (_proxy == proxy)
    return;
  [_proxy.scrollViewProxy removeObserver:self];
  _proxy = proxy;
  [_proxy.scrollViewProxy addObserver:self];
}

#pragma mark - CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewProxyDidSetScrollView:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  UpdateFullscreenWebViewProxyForReplacedScrollView(self.proxy, self.model);
}

@end
