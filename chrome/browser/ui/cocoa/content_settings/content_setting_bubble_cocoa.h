// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "content/public/common/media_stream_request.h"

class ContentSettingBubbleModel;
class ContentSettingBubbleWebContentsObserverBridge;
class ContentSettingMediaMenuModel;
@class InfoBubbleView;

namespace content {
class WebContents;
}

namespace content_setting_bubble {
// For every "show popup" button, remember the index of the popup tab contents
// it should open when clicked.
typedef std::map<NSButton*, int> PopupLinks;

// For every media menu button, remember the components assosiated with the
// menu button.
struct MediaMenuParts {
  MediaMenuParts(content::MediaStreamType type, NSTextField* label);
  ~MediaMenuParts();

  content::MediaStreamType type;
  NSTextField* label;  // Weak.
  scoped_ptr<ContentSettingMediaMenuModel> model;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaMenuParts);
};
typedef std::map<NSPopUpButton*, MediaMenuParts*> MediaMenuPartsMap;
}  // namespace content_setting_bubble

// Manages a "content blocked" bubble.
@interface ContentSettingBubbleController : BaseBubbleController {
 @private
  IBOutlet NSTextField* titleLabel_;
  IBOutlet NSMatrix* allowBlockRadioGroup_;

  IBOutlet NSButton* manageButton_;
  IBOutlet NSButton* doneButton_;
  IBOutlet NSButton* loadButton_;

  // The container for the bubble contents of the geolocation bubble.
  IBOutlet NSView* contentsContainer_;

  IBOutlet NSTextField* blockedResourcesField_;

  scoped_ptr<ContentSettingBubbleModel> contentSettingBubbleModel_;
  scoped_ptr<ContentSettingBubbleWebContentsObserverBridge> observerBridge_;
  content_setting_bubble::PopupLinks popupLinks_;
  content_setting_bubble::MediaMenuPartsMap mediaMenus_;
}

// Creates and shows a content blocked bubble. Takes ownership of
// |contentSettingBubbleModel| but not of the other objects.
+ (ContentSettingBubbleController*)
    showForModel:(ContentSettingBubbleModel*)contentSettingBubbleModel
     webContents:(content::WebContents*)webContents
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

// Callback for "load" (plug-ins, mixed script) button.
- (IBAction)load:(id)sender;

// Callback for "Learn More" link.
- (IBAction)learnMoreLinkClicked:(id)sender;

// Callback for "media menu" button.
- (IBAction)mediaMenuChanged:(id)sender;

@end

@interface ContentSettingBubbleController (TestingAPI)

// Returns the weak reference to the |mediaMenus_|.
- (content_setting_bubble::MediaMenuPartsMap*)mediaMenus;

@end
