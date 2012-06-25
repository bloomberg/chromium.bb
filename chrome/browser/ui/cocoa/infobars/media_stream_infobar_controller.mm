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
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "grit/generated_resources.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"

using l10n_util::GetNSStringWithFixup;

static const CGFloat kSpaceBetweenControls = 8;

InfoBar* MediaStreamInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  MediaStreamInfoBarController* infobar_controller =
      [[MediaStreamInfoBarController alloc] initWithDelegate:this
                                                       owner:owner];
  return new InfoBar(infobar_controller, this);
}

namespace {

// Puts |toMove| to the right or left of |anchor|.
void SizeAndPlaceControl(NSView* toModify, NSView* anchor, bool after) {
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:toModify];
  NSRect toModifyFrame = [toModify frame];
  NSRect anchorFrame = [anchor frame];
  if (after) {
    toModifyFrame.origin.x = NSMaxX(anchorFrame) + kSpaceBetweenControls;
  } else {
    toModifyFrame.origin.x = NSMinX(anchorFrame) - kSpaceBetweenControls -
        NSWidth(toModifyFrame);
  }
  [toModify setFrame:toModifyFrame];
}

}  // namespace


@interface MediaStreamInfoBarController(Private)

// Adds text for all buttons and text fields.
- (void)setLabelTexts;

// Populates the device menu with device id:s.
- (void)rebuildDeviceMenu;

// Arranges all buttons and pup-up menu relative each other.
- (void)arrangeInfobarLayout;

@end


@implementation MediaStreamInfoBarController

- (id)initWithDelegate:(MediaStreamInfoBarDelegate*)delegate
                 owner:(InfoBarTabHelper*)owner {
  if (self = [super initWithDelegate:delegate owner:owner]) {
    deviceMenuModel_.reset(new MediaStreamDevicesMenuModel(delegate));

    label1_.reset([[NSTextField alloc] init]);
    [label1_ setEditable:NO];
    [label1_ setDrawsBackground:NO];
    [label1_ setBordered:NO];

    [okButton_ setBezelStyle:NSTexturedRoundedBezelStyle];
    [cancelButton_ setBezelStyle:NSTexturedRoundedBezelStyle];

    deviceMenu_.reset([[NSPopUpButton alloc] init]);
    [deviceMenu_ setBezelStyle:NSTexturedRoundedBezelStyle];
    [deviceMenu_ setAutoresizingMask:NSViewMaxXMargin];
    [[deviceMenu_ cell] setArrowPosition:NSPopUpArrowAtBottom];
    NSMenu* menu = [deviceMenu_ menu];
    [menu setDelegate:nil];
    [menu setAutoenablesItems:NO];
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
  std::string audioId, videoId;
  deviceMenuModel_->GetSelectedDeviceId(
      content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE, &audioId);
  deviceMenuModel_->GetSelectedDeviceId(
      content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE, &videoId);
  bool alwaysAllow = deviceMenuModel_->always_allow();

  static_cast<MediaStreamInfoBarDelegate*>([self delegate])->Accept(
      audioId, videoId, alwaysAllow);

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
  // Get a frame to use as inital frame for all buttons.
  NSRect initFrame = [okButton_ frame];

  // Setup up the text label and add the device menu to the infobar.
  // TODO(mflodman) Use |label_| instead.
  [label1_ setFrame:[label_ frame]];
  [[label_ superview] replaceSubview:label_ with:label1_.get()];
  label_.reset();

  [deviceMenu_ setFrame:initFrame];
  [infoBarView_ addSubview:deviceMenu_];

  // Add text to all buttons and the text field.
  [self setLabelTexts];

  // Arrange the initial layout.
  [self arrangeInfobarLayout];

  // Build the device popup menu.
  [self rebuildDeviceMenu];

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

  string16 securityOrigin = UTF8ToUTF16(delegate->GetSecurityOrigin().spec());
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

  // Place |okButton_| to the right of the text field.
  SizeAndPlaceControl(okButton_, label1_, true);

  // Place |cancelButton| to the right of [okButton_|.
  SizeAndPlaceControl(cancelButton_, okButton_, true);

  // |deviceMenu_| is floating to the right, besides |closeButton_|.
  SizeAndPlaceControl(deviceMenu_, closeButton_, false);
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

  [menu addItem:[NSMenuItem separatorItem]];

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
  [GTMUILocalizerAndLayoutTweaker sizeToFitView:deviceMenu_];
}

- (void)didChangeFrame:(NSNotification*)notification {
  [self arrangeInfobarLayout];
}

@end  // implementation MediaStreamInfoBarController
