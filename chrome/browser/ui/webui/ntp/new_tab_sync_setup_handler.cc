// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_sync_setup_handler.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_switches.h"

void NewTabSyncSetupHandler::ShowSetupUI() {
  // Temporarily hide this feature behind a command line flag.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kSyncShowPromo))
    return;

  ProfileSyncService* service = web_ui_->GetProfile()->GetProfileSyncService();
  DCHECK(service);

  // If they user has already synced then don't show anything.
  if (service->HasSyncSetupCompleted())
    return;

  service->get_wizard().Step(SyncSetupWizard::GAIA_LOGIN);
}
