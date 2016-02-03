// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
#include "url/gurl.h"

class ExclusiveAccessManager;
class Profile;
@class GTMUILocalizerAndLayoutTweaker;
@class HyperlinkTextView;

// The ExclusiveAccessBubbleWindowController manages the bubble that informs the
// user of different exclusive access state like fullscreen mode, mouse lock,
// etc. Refer to EXCLUSIVE_ACCESS_BUBBLE_TYPE for the different possible
// conntents of the bubble.
@interface ExclusiveAccessBubbleWindowController
    : NSWindowController<NSTextViewDelegate, NSAnimationDelegate> {
 @private
  NSWindowController* owner_;                         // weak
  ExclusiveAccessManager* exclusive_access_manager_;  // weak
  Profile* profile_;                                  // weak
  GURL url_;
  ExclusiveAccessBubbleType bubbleType_;

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
  base::scoped_nsobject<HyperlinkTextView> exitLabel_;

  base::scoped_nsobject<NSTimer> hideTimer_;
  base::scoped_nsobject<NSAnimation> hideAnimation_;
};

// Initializes a new InfoBarController.
- (id)initWithOwner:(NSWindowController*)owner
    exclusive_access_manager:(ExclusiveAccessManager*)exclusive_access_manager
                     profile:(Profile*)profile
                         url:(const GURL&)url
                  bubbleType:(ExclusiveAccessBubbleType)bubbleType;

- (void)allow:(id)sender;
- (void)deny:(id)sender;

- (void)showWindow;
- (void)closeImmediately;

// Positions the exclusive access bubble in the top-center of the window.
- (void)positionInWindowAtTop:(CGFloat)maxY;

@end
