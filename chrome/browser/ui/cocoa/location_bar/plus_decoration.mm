// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/plus_decoration.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/location_bar/action_box_menu_bubble_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {
// The offset to apply to the menu so that it appears just attached to the
// right-hand side of the omnibox while slightly overlapping vertically.
const CGFloat kAnchorPointXOffset = 1.0;
const CGFloat kAnchorPointYOffset = 2.0;
}  // namespace

PlusDecoration::PlusDecoration(LocationBarViewMac* owner,
                               Browser* browser)
    : owner_(owner),
      browser_(browser),
      ALLOW_THIS_IN_INITIALIZER_LIST(controller_(browser, this)) {
  SetVisible(true);
  ResetIcon();
}

PlusDecoration::~PlusDecoration() {
}

NSPoint PlusDecoration::GetActionBoxAnchorPoint() {
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSRect bounds = [field bounds];
  NSPoint anchor = NSMakePoint(NSMaxX(bounds) - kAnchorPointXOffset,
                               NSMaxY(bounds) - kAnchorPointYOffset);
  return [field convertPoint:anchor toView:nil];
}

void PlusDecoration::ResetIcon() {
  SetIcons(
      IDR_ACTION_BOX_BUTTON_NORMAL,
      IDR_ACTION_BOX_BUTTON_HOVER,
      IDR_ACTION_BOX_BUTTON_PUSHED);
}

void PlusDecoration::SetTemporaryIcon(int image_id) {
  SetIcons(image_id, image_id, image_id);
}

bool PlusDecoration::AcceptsMousePress() {
  return true;
}

bool PlusDecoration::OnMousePressed(NSRect frame) {
  controller_.OnButtonClicked();
  return true;
}

NSString* PlusDecoration::GetToolTip() {
  return l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_ACTION_BOX_BUTTON);
}

void PlusDecoration::ShowMenu(scoped_ptr<ActionBoxMenuModel> menu_model) {
  // Controller for the menu attached to the plus decoration.
  // |menu_controller| will automatically release itself on close.
  NSWindow* parent = browser_->window()->GetNativeWindow();
  ActionBoxMenuBubbleController* menu_controller =
      [[ActionBoxMenuBubbleController alloc]
          initWithModel:menu_model.Pass()
           parentWindow:parent
             anchoredAt:[parent convertBaseToScreen:GetActionBoxAnchorPoint()]
                profile:browser_->profile()];

  [menu_controller showWindow:nil];
}

void PlusDecoration::SetIcons(int normal_id, int hover_id, int pressed_id) {
  SetNormalImage(OmniboxViewMac::ImageForResource(normal_id));
  SetHoverImage(OmniboxViewMac::ImageForResource(hover_id));
  SetPressedImage(OmniboxViewMac::ImageForResource(pressed_id));
}
