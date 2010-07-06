// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/base_bubble_controller.h"

class ContentSettingBubbleModel;
@class InfoBubbleView;

namespace content_blocked_bubble {
// For every "show popup" button, remember the index of the popup tab contents
// it should open when clicked.
typedef std::map<NSButton*, int> PopupLinks;
}

// Manages a "content blocked" bubble.
@interface ContentBlockedBubbleController : BaseBubbleController {
 @private
  IBOutlet NSTextField* titleLabel_;
  IBOutlet NSMatrix* allowBlockRadioGroup_;

  IBOutlet NSButton* manageButton_;
  IBOutlet NSButton* doneButton_;

  // The container for the bubble contents of the geolocation bubble.
  IBOutlet NSView* contentsContainer_;

  scoped_ptr<ContentSettingBubbleModel> contentSettingBubbleModel_;
  content_blocked_bubble::PopupLinks popupLinks_;
}

// Creates and shows a content blocked bubble. Takes ownership of
// |contentSettingBubbleModel| but not of the other objects.
+ (ContentBlockedBubbleController*)
    showForModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
    parentWindow:(NSWindow*)parentWindow
      anchoredAt:(NSPoint)anchoredAt;

// Callback for the "don't block / continue blocking" radio group.
- (IBAction)allowBlockToggled:(id)sender;

// Callback for "close" button.
- (IBAction)closeBubble:(id)sender;

// Callback for "manage" button.
- (IBAction)manageBlocking:(id)sender;

// Callback for "info" link.
- (IBAction)showMoreInfo:(id)sender;

@end
