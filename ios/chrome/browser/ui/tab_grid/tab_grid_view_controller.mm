// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Set a white background color to avoid flickers of black during startup.
  self.view.backgroundColor = [UIColor whiteColor];
}

@end
