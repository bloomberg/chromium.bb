// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

namespace browser {

void OpenAboutWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowAboutChrome(browser);
  browser->window()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowHistory(browser);
  browser->window()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowDownloads(browser);
  browser->window()->Show();
}

void OpenHelpWindow(Profile* profile, chrome::HelpSource source) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowHelp(browser, source);
  browser->window()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowSettings(browser);
  browser->window()->Show();
}

void OpenSyncSetupWindow(Profile* profile, SyncPromoUI::Source source) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowSyncSetup(browser, source);
  browser->window()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowClearBrowsingDataDialog(browser);
  browser->window()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowImportDialog(browser);
  browser->window()->Show();
}

void OpenInstantConfirmDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  chrome::ShowInstantConfirmDialog(browser);
  browser->window()->Show();
}

}  // namespace browser
