// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bar_button_activity_indicator.h"

#import "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/ui/ui_util.h"

@implementation BarButtonActivityIndicator {
  base::scoped_nsobject<UIActivityIndicatorView> activityIndicator_;
}

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    activityIndicator_.reset([[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleGray]);
    [activityIndicator_ setBackgroundColor:[UIColor clearColor]];
    [activityIndicator_ setHidesWhenStopped:YES];
    [activityIndicator_ startAnimating];
    [self addSubview:activityIndicator_];
  }
  return self;
}

- (void)dealloc {
  [activityIndicator_ stopAnimating];
  [super dealloc];
}

- (void)layoutSubviews {
  [super layoutSubviews];
  CGSize boundsSize = self.bounds.size;
  CGPoint center = CGPointMake(boundsSize.width / 2, boundsSize.height / 2);
  [activityIndicator_ setCenter:AlignPointToPixel(center)];
}

@end
