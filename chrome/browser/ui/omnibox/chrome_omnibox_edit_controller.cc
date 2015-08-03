// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"

ChromeOmniboxEditController::ChromeOmniboxEditController(
    CommandUpdater* command_updater)
    : command_updater_(command_updater) {}

ChromeOmniboxEditController::~ChromeOmniboxEditController() {}

void ChromeOmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition) {
  OmniboxEditController::OnAutocompleteAccept(destination_url, disposition,
                                              transition);
  if (command_updater_)
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);
}
