// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/plus_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#import "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace {
// The offset to apply to the menu so that it clears the bottom border of the
// omnibox.
const CGFloat kOmniboxYOffset = 7.0;
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
  ui::SimpleMenuModel menu_model(NULL);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // TODO(beaudoin): Use a platform-independent menu model once the Windows
  // patch introducing it lands. See: http://codereview.chromium.org/10533086/
  menu_model.InsertItemWithStringIdAt(0, IDC_CHROME_TO_MOBILE_PAGE,
                                      IDS_CHROME_TO_MOBILE);
  menu_model.SetIcon(0, *rb.GetImageSkiaNamed(IDR_MOBILE));
  menu_model.InsertItemWithStringIdAt(1, IDC_BOOKMARK_PAGE,
                                      IDS_BOOKMARK_STAR);
  menu_model.SetIcon(1, *rb.GetImageSkiaNamed(IDR_STAR));

  // Controller for the menu attached to the plus decoration.
  scoped_nsobject<MenuController> menu_controller(
      [[MenuController alloc] initWithModel:&menu_model
                     useWithPopUpButtonCell:YES]);

  NSMenu* menu = [menu_controller menu];

  // Align the menu popup to that its top-right corner matches the bottom-right
  // corner of the omnibox.
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();

  NSRect popUpFrame = [field bounds];
  popUpFrame.origin.x = NSMaxX([field bounds]) - menu.size.width;
  popUpFrame.size.width = menu.size.width;

  // Attach the menu to a slightly higher box, to clear the omnibox border.
  popUpFrame.size.height += kOmniboxYOffset;

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
