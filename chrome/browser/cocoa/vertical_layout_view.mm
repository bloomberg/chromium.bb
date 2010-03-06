// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/vertical_layout_view.h"

@interface VerticalLayoutView(PrivateMethods)
- (void)layoutChildren;
@end

@implementation VerticalLayoutView

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // Turn auto resizing off, we'll be laying out our children programatically.
    [self setAutoresizesSubviews:NO];
    [self setAutoresizingMask:NSViewNotSizable];
  }

  return self;
}

// Flip the coordinate system to arrange child views from top to bottom
// with top at 0, increasing down.  This simplifies the logic and plays
// well with containing scroll views.
- (BOOL)isFlipped {
  return YES;
}

// Override the default |viewWillDraw| to indicate to drawing machinery proper
// arrangement of subviews.
- (void)viewWillDraw {
  // Reposition child views prior to super's descent into its |viewWillDraw|
  // pass.
  [self layoutChildren];

  // Default descent into subviews.
  [super viewWillDraw];

  // Adjust children again to account for any modifications made during the
  // prior descent.  Most importantly we resize our own frame to properly
  // adjust any containing scroll view.
  [self layoutChildren];
}

@end

@implementation VerticalLayoutView(PrivateMethods)

// This method traverses the immediate subviews measuring their height and
// adjusting their frames so they are arranged vertically ordered relative
// to their sibling views.  Note the dependency here on the |isFlipped|
// state.  This code assumes |isFlipped| is YES.
- (void)layoutChildren {
  NSArray* children = [self subviews];
  int childCount = [children count];

  CGFloat yPosition = 0.0;
  for (int i = childCount-1; i >= 0; --i) {
    NSView* child = [children objectAtIndex:i];
    [child setFrameOrigin:NSMakePoint([child frame].origin.x, yPosition)];
    yPosition += [child frame].size.height;
  }

  // Resize self to reflect vertical extent of children.
  [self setFrame:NSMakeRect([self frame].origin.x,
                            [self frame].origin.y,
                            [self frame].size.width,
                            yPosition)];
}

@end
