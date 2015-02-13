// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"

void OmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition) {
  destination_url_ = destination_url;
  disposition_ = disposition;
  transition_ = transition;
  if (command_updater_)
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
}

OmniboxEditController::OmniboxEditController(CommandUpdater* command_updater)
    : command_updater_(command_updater),
      disposition_(CURRENT_TAB),
      transition_(ui::PageTransitionFromInt(
          ui::PAGE_TRANSITION_TYPED |
          ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
}

OmniboxEditController::~OmniboxEditController() {
}
