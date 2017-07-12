// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BubbleViewController ()
@property(nonatomic, readonly) NSString* text;
@property(nonatomic, readonly) BubbleArrowDirection arrowDirection;
@property(nonatomic, readonly) BubbleAlignment alignment;
@end

@implementation BubbleViewController

@synthesize text = _text;
@synthesize arrowDirection = _arrowDirection;
@synthesize alignment = _alignment;

- (instancetype)initWithText:(NSString*)text
                   direction:(BubbleArrowDirection)arrowDirection
                   alignment:(BubbleAlignment)alignment {
  self = [super initWithNibName:nil bundle:nil];
  _text = text;
  _arrowDirection = arrowDirection;
  _alignment = alignment;
  return self;
}

- (void)loadView {
  self.view = [[BubbleView alloc] initWithText:self.text
                                     direction:self.arrowDirection
                                     alignment:self.alignment];
}

- (void)animateContentIn {
  NOTIMPLEMENTED();
}

- (void)dismissAnimated:(BOOL)animated {
  NOTIMPLEMENTED();
}

@end
