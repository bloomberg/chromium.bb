// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/plus_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

PlusDecoration::PlusDecoration(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
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
  // TODO(macourteau): trigger the menu when caitkp@ and beaudoin@'s CL is
  // ready.
  return true;
}

NSString* PlusDecoration::GetToolTip() {
  return tooltip_.get();
}
