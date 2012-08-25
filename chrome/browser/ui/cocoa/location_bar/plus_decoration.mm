// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/plus_decoration.h"

#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
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
    CommandUpdater* command_updater, Browser* browser)
    : owner_(owner),
      command_updater_(command_updater),
      browser_(browser) {
  SetVisible(true);

  const int image_id = IDR_ACTION_BOX_BUTTON;
  SetImage(OmniboxViewMac::ImageForResource(image_id));
  const int tip_id = IDS_TOOLTIP_ACTION_BOX_BUTTON;
  tooltip_.reset([l10n_util::GetNSStringWithFixup(tip_id) retain]);
}

PlusDecoration::~PlusDecoration() {
}

bool PlusDecoration::AcceptsMousePress() {
  return true;
}

bool PlusDecoration::OnMousePressed(NSRect frame) {
  ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      browser_->profile())->extension_service();
  ActionBoxMenuModel menu_model(browser_, extension_service);

  // Controller for the menu attached to the plus decoration.
  scoped_nsobject<MenuController> menu_controller(
      [[MenuController alloc] initWithModel:&menu_model
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
  return true;
}

NSString* PlusDecoration::GetToolTip() {
  return tooltip_.get();
}

NSPoint PlusDecoration::GetActionBoxAnchorPoint() {
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSRect bounds = [field bounds];
  return NSMakePoint(NSMaxX(bounds), NSMaxY(bounds));
}
