// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/infobars/media_stream_infobar_controller.h"

#import <Cocoa/Cocoa.h>
#include <string>

#import "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media/media_stream_devices_menu_model.h"
#include "chrome/browser/ui/cocoa/hover_close_button.h"
#include "chrome/browser/ui/cocoa/infobars/infobar.h"
#import "chrome/browser/ui/cocoa/infobars/infobar_utilities.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"

using InfoBarUtilities::CreateLabel;
using InfoBarUtilities::MoveControl;
using InfoBarUtilities::VerifyControlOrderAndSpacing;
using l10n_util::GetNSStringWithFixup;

InfoBar* MediaStreamInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  MediaStreamInfoBarController* infobar_controller =
      [[MediaStreamInfoBarController alloc] initWithDelegate:this
                                                       owner:owner];
  return new InfoBar(infobar_controller, this);
}


@interface MediaStreamInfoBarController(Private)

// Adds text for all buttons and text fields.
- (void)setLabelTexts;

// Populates the device menu with device id:s.
- (void)rebuildDeviceMenu;

// Arranges all buttons and pup-up menu relative each other.
- (void)arrangeInfobarLayout;

// Create all the components for the infobar.
- (void)constructView;

- (void)showDeviceMenuTitle:(BOOL)showTitle;
@end


@implementation MediaStreamInfoBarController

- (id)initWithDelegate:(MediaStreamInfoBarDelegate*)delegate
                 owner:(InfoBarService*)owner {
  if (self = [super initWithDelegate:delegate owner:owner]) {
    deviceMenuModel_.reset(new MediaStreamDevicesMenuModel(delegate));
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [deviceMenu_ removeAllItems];
  [super dealloc];
}

// Called when "Allow" button is pressed.
- (void)ok:(id)sender {
  static_cast<MediaStreamInfoBarDelegate*>([self delegate])->Accept();

  // Remove the infobar, we're done.
  [super removeSelf];
}

// Called when "Deny" button is pressed.
- (void)cancel:(id)sender {
  static_cast<MediaStreamInfoBarDelegate*>([self delegate])->Deny();

  // Remove the infobar, we're done.
  [super removeSelf];
}

- (IBAction)deviceMenuChanged:(id)item {
  // Notify the menu model about the change and rebuild the menu.
  deviceMenuModel_->ExecuteCommand([item tag]);
  [self rebuildDeviceMenu];
}

- (void)addAdditionalControls {
  // Create all the components for the infobar.
  [self constructView];

  // Get layout information from the NIB.
  NSRect cancelButtonFrame = [cancelButton_ frame];
  NSRect okButtonFrame = [okButton_ frame];

  // Verify Cancel button is on the left of OK button.
  DCHECK(NSMaxX(cancelButtonFrame) < NSMinX(okButtonFrame));
  spaceBetweenControls_ =  NSMinX(okButtonFrame) - NSMaxX(cancelButtonFrame);

  // Arrange the initial layout.
  [self arrangeInfobarLayout];

  // Make sure we get notified of infobar view size changes.
  // TODO(mflodman) Find if there is a way to use 'setAutorezingMask' instead.
  [infoBarView_ setPostsFrameChangedNotifications:YES];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(didChangeFrame:)
             name:NSViewFrameDidChangeNotification
           object:infoBarView_];
}

- (void)infobarWillClose {
  [self disablePopUpMenu:[deviceMenu_ menu]];
  [super infobarWillClose];
}

- (void)setLabelTexts {
  MediaStreamInfoBarDelegate* delegate =
      static_cast<MediaStreamInfoBarDelegate*>([self delegate]);

  // Get the requested media type(s) and add corresponding text to the text
  // field.
  int messageId = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
  DCHECK(delegate->HasAudio() || delegate->HasVideo());
  if (!delegate->HasAudio())
    messageId = IDS_MEDIA_CAPTURE_VIDEO_ONLY;
  else if (!delegate->HasVideo())
    messageId = IDS_MEDIA_CAPTURE_AUDIO_ONLY;

  string16 securityOrigin = UTF8ToUTF16(delegate->GetSecurityOriginSpec());
  NSString* text = l10n_util::GetNSStringF(messageId, securityOrigin);
  [label1_ setStringValue:text];

  // Add text to the buttons.
  [okButton_ setTitle:GetNSStringWithFixup(IDS_MEDIA_CAPTURE_ALLOW)];
  [cancelButton_ setTitle:GetNSStringWithFixup(IDS_MEDIA_CAPTURE_DENY)];

  // Populating |deviceMenu_| is handled separately.
}

- (void)arrangeInfobarLayout {
  // Set correct size for text frame.
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:label1_];

  // From left to right: label_, cancelButton_ and okButton_.
  MoveControl(label1_, cancelButton_, spaceBetweenControls_, true);
  MoveControl(cancelButton_, okButton_, spaceBetweenControls_, true);

  // Build the device option popup menu.
  [deviceMenu_ setHidden:NO];
  [self showDeviceMenuTitle:YES];
  MoveControl(closeButton_, deviceMenu_, spaceBetweenControls_, false);

  // Hide the title of deviceMenu_ or even the whole menu when it overlaps
  // with the okButton_.
  if (!VerifyControlOrderAndSpacing(okButton_, deviceMenu_))  {
    NSRect oldFrame = [deviceMenu_ frame];
    oldFrame.size.width = NSHeight(oldFrame);
    [deviceMenu_ setFrame:oldFrame];
    [self showDeviceMenuTitle:NO];
    MoveControl(closeButton_, deviceMenu_, spaceBetweenControls_, false);
    if (!VerifyControlOrderAndSpacing(okButton_, deviceMenu_)) {
      [deviceMenu_ setHidden:YES];
    }
  }
}

- (void)constructView {
  // Use the ok button frame as inital frame for all components.
  NSRect initFrame = [okButton_ frame];

  // Setup the text label and add the device menu to the infobar.
  // TODO(mflodman) Use |label_| instead.
  label1_.reset([[NSTextField alloc] init]);
  [label1_ setEditable:NO];
  [label1_ setDrawsBackground:NO];
  [label1_ setBordered:NO];
  [label1_ setFrame:[label_ frame]];
  [[label_ superview] replaceSubview:label_ with:label1_];
  label_.reset();

  [okButton_ setBezelStyle:NSTexturedRoundedBezelStyle];
  [cancelButton_ setBezelStyle:NSTexturedRoundedBezelStyle];

  // Add text to all buttons and the text field.
  [self setLabelTexts];

  // Setup the device options menu.
  deviceMenu_.reset([[NSPopUpButton alloc] initWithFrame:initFrame
                                               pullsDown:YES]);
  [deviceMenu_ setBezelStyle:NSTexturedRoundedBezelStyle];
  [deviceMenu_ setAutoresizingMask:NSViewMinXMargin];
  [self rebuildDeviceMenu];
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:deviceMenu_];
  NSMenu* menu = [deviceMenu_ menu];
  [menu setDelegate:nil];
  [menu setAutoenablesItems:NO];

  // Add "options" popup z-ordered below all other controls so when we
  // resize the toolbar it doesn't hide them.
  [infoBarView_ addSubview:deviceMenu_
                positioned:NSWindowBelow
                relativeTo:nil];
}

- (void)rebuildDeviceMenu {
  // Remove all old menu items and rebuild from scratch.
  [deviceMenu_ removeAllItems];
  NSMenu* menu = [deviceMenu_ menu];

  // Add title item.
  NSString* menuTitle = GetNSStringWithFixup(
      IDS_MEDIA_CAPTURE_DEVICES_MENU_TITLE);
  NSMenuItem* titleItem =
      [menu addItemWithTitle:menuTitle
                      action:NULL
               keyEquivalent:@""];
  [titleItem setState:NSOffState];

  // Add all capture devices.
  for (int i = 0; i < deviceMenuModel_->GetItemCount(); ++i) {
    if (deviceMenuModel_->GetCommandIdAt(i) == -1) {
      [menu addItem:[NSMenuItem separatorItem]];
    } else {
      NSString* title =
          base::SysUTF16ToNSString(deviceMenuModel_->GetLabelAt(i));
      NSMenuItem* item = [menu addItemWithTitle:title
                                         action:@selector(deviceMenuChanged:)
                                  keyEquivalent:@""];
      [item setTarget:self];
      [item setTag:deviceMenuModel_->GetCommandIdAt(i)];
      if (deviceMenuModel_->IsItemCheckedAt(i)) {
        [item setState:NSOnState];
      }
      [item setEnabled:deviceMenuModel_->IsEnabledAt(i)];
    }
  }
}

- (void)showDeviceMenuTitle:(BOOL)showTitle {
  if (showTitle) {
    NSString* menuTitle = GetNSStringWithFixup(
        IDS_MEDIA_CAPTURE_DEVICES_MENU_TITLE);
    [[[deviceMenu_ menu] itemAtIndex:0] setTitle:menuTitle];
    [[deviceMenu_ cell] setArrowPosition:NSPopUpArrowAtBottom];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:deviceMenu_];
  } else {
    [[[deviceMenu_ menu] itemAtIndex:0] setTitle:@""];
    [[deviceMenu_ cell] setArrowPosition:NSPopUpArrowAtCenter];
  }
}

- (void)didChangeFrame:(NSNotification*)notification {
  [self arrangeInfobarLayout];
}

@end  // implementation MediaStreamInfoBarController
