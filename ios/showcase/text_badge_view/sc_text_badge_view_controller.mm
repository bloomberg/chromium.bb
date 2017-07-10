// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/text_badge_view/sc_text_badge_view_controller.h"

#import "ios/chrome/browser/ui/reading_list/text_badge_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCTextBadgeViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor grayColor];
  TextBadgeView* textBadge = [[TextBadgeView alloc] initWithText:@"TEXT"];
  [self.view addSubview:textBadge];
  textBadge.bounds = CGRectMake(0.0f, 0.0f, 100.0f, 100.0f);
  textBadge.center = self.view.center;
}

@end
