// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/common/content_settings_types.h"

class BubbleCloser;
@class InfoBubbleView;
class Profile;
class TabContents;

namespace content_blocked_bubble {
// For every "show popup" button, remember which popup tab contents it should
// open when clicked.
typedef std::map<NSButton*, TabContents*> PopupLinks;
}

// Manages a "content blocked" bubble.
@interface ContentBlockedBubbleController
    : NSWindowController<NSWindowDelegate> {
 @private
  NSWindow* parentWindow_;  // weak
  NSPoint anchor_;
  IBOutlet InfoBubbleView* bubble_;  // to set arrow position

  IBOutlet NSTextField* titleLabel_;
  IBOutlet NSMatrix* allowBlockRadioGroup_;

  ContentSettingsType settingsType_;
  std::string host_;
  scoped_nsobject<NSString> displayHost_;
  Profile* profile_;
  TabContents* tabContents_;
  content_blocked_bubble::PopupLinks popupLinks_;

  // Resets |tabContents_| to null if the object |tabContents_| is pointing to
  // gets destroyed.
  scoped_ptr<BubbleCloser> closer_;
}

// Creates and shows a content blocked bubble.
+ (ContentBlockedBubbleController*)showForType:(ContentSettingsType)settingsType
                                  parentWindow:(NSWindow*)parentWindow
                                    anchoredAt:(NSPoint)anchoredAt
                                          host:(const std::string&)host
                                   displayHost:(NSString*)displayHost
                                   tabContents:(TabContents*)tabContents
                                       profile:(Profile*)profile;

// Returns the tab contents this bubble is for.
@property(nonatomic, assign) TabContents* tabContents;

// Callback for the "don't block / continue blocking" radio group.
- (IBAction)allowBlockToggled:(id)sender;

// Callback for "close" button.
- (IBAction)closeBubble:(id)sender;

// Callback for "manage" button.
- (IBAction)manageBlocking:(id)sender;

@end
