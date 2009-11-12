// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

enum BubbleArrowLocation {
  kTopLeft,
  kTopRight,
};

// Content view for a bubble with an arrow showing arbitrary content.
// This is where nonrectangular drawing happens.
@interface InfoBubbleView : NSView {
 @private
  BubbleArrowLocation arrowLocation_;
}

@property (assign, nonatomic) BubbleArrowLocation arrowLocation;

@end
