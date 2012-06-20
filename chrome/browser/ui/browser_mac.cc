// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"

namespace browser {

void OpenAboutWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenAboutChromeDialog();
  browser->window()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowHistoryTab();
  browser->window()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowDownloadsTab();
  browser->window()->Show();
}

void OpenHelpWindow(Profile* profile, Browser::HelpSource source) {
  Browser* browser = Browser::Create(profile);
  browser->ShowHelpTab(source);
  browser->window()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenOptionsDialog();
  browser->window()->Show();
}

void OpenSyncSetupWindow(Profile* profile, SyncPromoUI::Source source) {
  Browser* browser = Browser::Create(profile);
  browser->ShowSyncSetup(source);
  browser->window()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenClearBrowsingDataDialog();
  browser->window()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenImportSettingsDialog();
  browser->window()->Show();
}

void OpenInstantConfirmDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenInstantConfirmDialog();
  browser->window()->Show();
}

}  // namespace browser
