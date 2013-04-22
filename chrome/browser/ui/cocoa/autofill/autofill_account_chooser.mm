// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/autofill_account_chooser.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_constants.h"
#import "chrome/browser/ui/cocoa/autofill/down_arrow_popup_menu_cell.h"
#import "chrome/browser/ui/cocoa/hyperlink_button_cell.h"
#import "chrome/browser/ui/cocoa/menu_button.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"

#pragma mark Helper functions

// Adds an item with the specified properties to |menu|.
void AddMenuItem(NSMenu *menu, id target, SEL selector, NSString* title,
    int tag, bool enabled, bool checked) {
  if (tag == -1) {
    [menu addItem:[NSMenuItem separatorItem]];
    return;
  }

  scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
    initWithTitle:title
           action:selector
    keyEquivalent:@""]);
  [item setTag:tag];
  [menu addItem:item];
  [item setTarget:target];
  if (checked)
    [item setState:NSOnState];
  [item setEnabled:enabled];
}

#pragma mark AutofillAccountChooser

@interface AutofillAccountChooser (Private)
- (void)performLayout;
@end

@implementation AutofillAccountChooser

- (id)initWithFrame:(NSRect)frame
     controller:(autofill::AutofillDialogController*)controller {
  if ((self = [super initWithFrame:frame])) {
    controller_ = controller;

    icon_.reset([[NSImageView alloc] initWithFrame:NSZeroRect]);
    [icon_ setImageFrameStyle:NSImageFrameNone];

    link_.reset([[HyperlinkButtonCell buttonWithString:
        base::SysUTF16ToNSString(controller_->SignInLinkText())] retain]);
    [link_ setAction:@selector(linkClicked:)];
    [link_ setTarget:self];
    [[link_ cell] setUnderlineOnHover:YES];
    [[link_ cell] setTextColor:
        gfx::SkColorToCalibratedNSColor(chrome_style::GetLinkColor())];

    popup_.reset([[MenuButton alloc] initWithFrame:NSZeroRect]);
    scoped_nsobject<DownArrowPopupMenuCell> popupCell(
        [[DownArrowPopupMenuCell alloc] initTextCell:@""]);
    [popup_ setCell:popupCell];

    [popup_ setOpenMenuOnClick:YES];
    [popup_ setBordered:NO];
    [popup_ setShowsBorderOnlyWhileMouseInside:NO];
    NSImage* popupImage = ui::ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(IDR_MENU_DROPARROW).ToNSImage();
    [popupCell setImage:popupImage
        forButtonState:image_button_cell::kDefaultState];

    scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
    [menu setAutoenablesItems:NO];
    [popup_ setAttachedMenu:menu];

    [self setSubviews:@[link_, popup_, icon_]];
    [self update];
  }
  return self;
}

- (void)optionsMenuChanged:(id)sender {
  ui::MenuModel* menuModel = controller_->MenuModelForAccountChooser();
  DCHECK(menuModel);
  menuModel->ActivatedAt([sender tag]);
}

- (void)linkClicked:(id)sender {
  controller_->StartSignInFlow();
}

- (void)update {
  NSImage* iconImage = controller_->AccountChooserImage().AsNSImage();
  [icon_ setImage:iconImage];

  // Set title.
  NSString* popupTitle =
      base::SysUTF16ToNSString(controller_->AccountChooserText());
  [popup_ setTitle:popupTitle];

  // populate menu
  NSMenu* accountMenu = [popup_ attachedMenu];
  [accountMenu removeAllItems];
  // See menu_button.h for documentation on why this is needed.
  [accountMenu addItemWithTitle:@"" action:nil keyEquivalent:@""];
  ui::MenuModel* model = controller_->MenuModelForAccountChooser();
  if (model) {
    for (int i = 0; i < model->GetItemCount(); ++i) {
      AddMenuItem(accountMenu,
                  self,
                  @selector(optionsMenuChanged:),
                  base::SysUTF16ToNSString(model->GetLabelAt(i)),
                  model->GetCommandIdAt(i),
                  model->IsEnabledAt(i),
                  model->IsItemCheckedAt(i));
    }
  }

  bool showLink = !model;
  [popup_ setHidden:showLink];
  [link_ setHidden:!showLink];

  [self performLayout];
}

- (void)performLayout {
  NSRect bounds = [self bounds];

  NSControl* activeControl = [link_ isHidden] ? popup_.get() : link_.get();
  [activeControl sizeToFit];
  NSRect frame = [activeControl frame];
  frame.origin.x = NSMaxX(bounds) - NSWidth(frame);
  [activeControl setFrame:frame];

  [icon_ setFrameSize:[[icon_ image] size]];
  frame.origin.x -= NSWidth([icon_ frame]) + kAroundTextPadding;
  frame.size.width = NSWidth([icon_ frame]);
  [icon_ setFrame:frame];
}

@end

