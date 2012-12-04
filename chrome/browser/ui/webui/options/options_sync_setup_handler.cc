// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/options_sync_setup_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "content/public/browser/web_ui.h"

namespace options {

OptionsSyncSetupHandler::OptionsSyncSetupHandler(
    ProfileManager* profile_manager) : SyncSetupHandler(profile_manager) {
}

OptionsSyncSetupHandler::~OptionsSyncSetupHandler() {
}

void OptionsSyncSetupHandler::ShowSetupUI() {
  // Show the Sync Setup page.
  web_ui()->CallJavascriptFunction("OptionsPage.navigateToPage",
                                   base::StringValue("syncSetup"));
}

}  // namespace options
