// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_MAC_H_
#define CHROME_BROWSER_UI_BROWSER_MAC_H_
#pragma once

#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"

class Profile;

namespace browser {

// Open a new window with history/downloads/help/options (needed on Mac when
// there are no windows).
void OpenAboutWindow(Profile* profile);
void OpenHistoryWindow(Profile* profile);
void OpenDownloadsWindow(Profile* profile);
void OpenHelpWindow(Profile* profile, chrome::HelpSource source);
void OpenOptionsWindow(Profile* profile);
void OpenSyncSetupWindow(Profile* profile, SyncPromoUI::Source source);
void OpenClearBrowsingDataDialogWindow(Profile* profile);
void OpenImportSettingsDialogWindow(Profile* profile);
void OpenInstantConfirmDialogWindow(Profile* profile);

}  // namespace browser

#endif  // CHROME_BROWSER_UI_BROWSER_MAC_H_
