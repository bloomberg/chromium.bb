// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/fullscreen/fullscreen_exit_bubble_type.h"
#include "url/gurl.h"

@class BrowserWindowController;
class Browser;
@class GTMUILocalizerAndLayoutTweaker;

// The FullscreenExitBubbleController manages the bubble that tells the user
// how to escape fullscreen mode. The bubble only appears when a tab requests
// fullscreen mode via webkitRequestFullScreen().
@interface FullscreenExitBubbleController :
    NSWindowController<NSTextViewDelegate, NSAnimationDelegate> {
 @private
  BrowserWindowController* owner_;  // weak
  Browser* browser_; // weak
  GURL url_;
  FullscreenExitBubbleType bubbleType_;

 @protected
  IBOutlet NSTextField* exitLabelPlaceholder_;
  IBOutlet NSTextField* messageLabel_;
  IBOutlet NSButton* allowButton_;
  IBOutlet NSButton* denyButton_;
  IBOutlet GTMUILocalizerAndLayoutTweaker* tweaker_;

  // Text fields don't work as well with embedded links as text views, but
  // text views cannot conveniently be created in IB. The xib file contains
  // a text field |exitLabelPlaceholder_| that's replaced by this text view
  // |exitLabel_| in -awakeFromNib.
  base::scoped_nsobject<NSTextView> exitLabel_;

  base::scoped_nsobject<NSTimer> hideTimer_;
  base::scoped_nsobject<NSAnimation> hideAnimation_;
};

// Initializes a new InfoBarController.
- (id)initWithOwner:(BrowserWindowController*)owner
            browser:(Browser*)browser
                url:(const GURL&)url
         bubbleType:(FullscreenExitBubbleType)bubbleType;

- (void)allow:(id)sender;
- (void)deny:(id)sender;

- (void)showWindow;
- (void)closeImmediately;

// Positions the fullscreen exit bubble in the top-center of the window.
- (void)positionInWindowAtTop:(CGFloat)maxY width:(CGFloat)maxWidth;

@end
