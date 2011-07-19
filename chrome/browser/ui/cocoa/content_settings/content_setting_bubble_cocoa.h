// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class ContentSettingBubbleModel;
@class InfoBubbleView;

namespace content_setting_bubble {
// For every "show popup" button, remember the index of the popup tab contents
// it should open when clicked.
typedef std::map<NSButton*, int> PopupLinks;
}

// Manages a "content blocked" bubble.
@interface ContentSettingBubbleController : BaseBubbleController {
 @private
  IBOutlet NSTextField* titleLabel_;
  IBOutlet NSMatrix* allowBlockRadioGroup_;

  IBOutlet NSButton* manageButton_;
  IBOutlet NSButton* doneButton_;
  IBOutlet NSButton* loadAllPluginsButton_;

  // The container for the bubble contents of the geolocation bubble.
  IBOutlet NSView* contentsContainer_;

  // The info button of the cookies bubble.
  IBOutlet NSButton* infoButton_;

  IBOutlet NSTextField* blockedResourcesField_;

  scoped_ptr<ContentSettingBubbleModel> contentSettingBubbleModel_;
  content_setting_bubble::PopupLinks popupLinks_;
}

// Creates and shows a content blocked bubble. Takes ownership of
// |contentSettingBubbleModel| but not of the other objects.
+ (ContentSettingBubbleController*)
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

// Callback for "load all plugins" button.
- (IBAction)loadAllPlugins:(id)sender;

@end
