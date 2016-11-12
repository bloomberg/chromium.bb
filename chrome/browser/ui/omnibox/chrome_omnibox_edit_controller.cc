// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "components/toolbar/toolbar_model.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#endif

ChromeOmniboxEditController::ChromeOmniboxEditController(
    CommandUpdater* command_updater)
    : command_updater_(command_updater) {}

ChromeOmniboxEditController::~ChromeOmniboxEditController() {}

void ChromeOmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type) {
  OmniboxEditController::OnAutocompleteAccept(destination_url, disposition,
                                              transition, match_type);
  if (command_updater_)
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MaybeShowExtensionControlledSearchNotification(
      GetWebContents(), match_type);
#endif
}

void ChromeOmniboxEditController::OnInputInProgress(bool in_progress) {
  GetToolbarModel()->set_input_in_progress(in_progress);
  UpdateWithoutTabRestore();
}
