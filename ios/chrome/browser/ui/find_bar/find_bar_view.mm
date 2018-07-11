// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/find_bar/find_bar_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FindBarView

@synthesize inputField = _inputField;
@synthesize previousButton = _previousButton;
@synthesize nextButton = _nextButton;
@synthesize closeButton = _closeButton;

- (void)updateResultsLabelWithText:(NSString*)text {
  // TODO(crbug.com/805504): Implement this.
}

@end
