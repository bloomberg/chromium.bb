// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/omnibox/location_bar_view_controller.h"

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LocationBarViewController ()
@property(nonatomic, readwrite, strong) OmniboxTextFieldIOS* omnibox;
@end

@implementation LocationBarViewController

@synthesize omnibox = _omnibox;

- (instancetype)init {
  if ((self = [super init])) {
    UIColor* textColor = [UIColor blackColor];
    UIColor* tintColor = nil;
    _omnibox =
        [[OmniboxTextFieldIOS alloc] initWithFrame:CGRectZero
                                              font:[UIFont systemFontOfSize:14]
                                         textColor:textColor
                                         tintColor:tintColor];
  }
  return self;
}

- (void)viewDidLoad {
  self.omnibox.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.omnibox.frame = self.view.bounds;
  [self.view addSubview:self.omnibox];
}

@end
