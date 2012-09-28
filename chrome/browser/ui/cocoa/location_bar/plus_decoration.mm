// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/plus_decoration.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {
// The offset to apply to the menu so that it clears the bottom border of the
// omnibox.
const CGFloat kAnchorPointYOffset = 4.0;
const CGFloat kAnchorPointFrameHeight = 23.0;
}  // namespace

PlusDecoration::PlusDecoration(LocationBarViewMac* owner,
                               Browser* browser)
    : owner_(owner),
      browser_(browser),
      ALLOW_THIS_IN_INITIALIZER_LIST(controller_(browser, this)) {
  SetVisible(true);
  SetImage(OmniboxViewMac::ImageForResource(IDR_ACTION_BOX_BUTTON));
}

PlusDecoration::~PlusDecoration() {
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

NSPoint PlusDecoration::GetActionBoxAnchorPoint() {
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSRect bounds = [field bounds];
  return NSMakePoint(NSMaxX(bounds), NSMaxY(bounds));
}

void PlusDecoration::ShowMenu(scoped_ptr<ActionBoxMenuModel> menu_model) {
  // Controller for the menu attached to the plus decoration.
  scoped_nsobject<MenuController> menu_controller(
      [[MenuController alloc] initWithModel:menu_model.get()
                     useWithPopUpButtonCell:YES]);

  NSMenu* menu = [menu_controller menu];

  // Align the menu popup to that its top-right corner matches the bottom-right
  // corner of the omnibox.
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSPoint point = GetActionBoxAnchorPoint();
  NSRect popUpFrame = NSMakeRect(point.x - menu.size.width,
      kAnchorPointYOffset, menu.size.width, kAnchorPointFrameHeight);
  scoped_nsobject<NSPopUpButtonCell> pop_up_cell(
      [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:YES]);
  DCHECK(pop_up_cell.get());

  [pop_up_cell setMenu:menu];
  [pop_up_cell selectItem:nil];
  [pop_up_cell attachPopUpWithFrame:popUpFrame inView:field];
  [pop_up_cell performClickWithFrame:popUpFrame inView:field];
}
