// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "googleurl/src/gurl.h"

@class AnimatableView;
class TabContentsWrapper;
@class BrowserWindowController;
class Browser;

// The FullscreenExitBubbleController manages the bubble that tells the user
// how to escape fullscreen mode. The bubble only appears when a tab requests
// fullscreen mode via webkitRequestFullScreen().
@interface FullscreenExitBubbleController :
    NSViewController<NSTextViewDelegate> {
 @private
  BrowserWindowController* owner_;  // weak
  Browser* browser_; // weak

 @protected
  IBOutlet NSTextField* exitLabelPlaceholder_;

  // Text fields don't work as well with embedded links as text views, but
  // text views cannot conveniently be created in IB. The xib file contains
  // a text field |exitLabelPlaceholder_| that's replaced by this text view
  // |exitLabel_| in -awakeFromNib.
  scoped_nsobject<NSTextView> exitLabel_;

  scoped_nsobject<NSTimer> hideTimer_;
  scoped_nsobject<NSAnimation> hideAnimation_;
};

- (id)initWithOwner:(BrowserWindowController*)owner browser:(Browser*)browser;

// Positions the fullscreen exit bubble in the top-center of the window.
- (void)positionInWindowAtTop:(CGFloat)maxY width:(CGFloat)maxWidth;

// Returns a pointer to this controller's view, cast as an AnimatableView.
- (AnimatableView*)animatableView;

@end
