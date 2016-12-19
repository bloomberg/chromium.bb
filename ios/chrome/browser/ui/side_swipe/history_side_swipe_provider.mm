// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/history_side_swipe_provider.h"

@interface HistorySideSwipeProvider () {
  // Keep a reference to detach before deallocing.
  TabModel* _tabModel;  // weak
}

@end

@implementation HistorySideSwipeProvider
- (instancetype)initWithTabModel:(TabModel*)tabModel {
  self = [super init];
  if (self) {
    _tabModel = tabModel;
  }
  return self;
}

- (BOOL)canGoBack {
  return [[_tabModel currentTab] canGoBack];
}

- (BOOL)canGoForward {
  return [[_tabModel currentTab] canGoForward];
}

- (void)goForward:(web::WebState*)webState {
  [[_tabModel currentTab] goForward];
}

- (void)goBack:(web::WebState*)webState {
  [[_tabModel currentTab] goBack];
}

- (UIImage*)paneIcon {
  return [UIImage imageNamed:@"side_swipe_navigation_back"];
}

- (BOOL)rotateForwardIcon {
  return YES;
}

@end
