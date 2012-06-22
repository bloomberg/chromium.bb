// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

namespace chrome {

void OpenAboutWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowAboutChrome(browser);
  browser->window()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowHistory(browser);
  browser->window()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowDownloads(browser);
  browser->window()->Show();
}

void OpenHelpWindow(Profile* profile, HelpSource source) {
  Browser* browser = Browser::Create(profile);
  ShowHelp(browser, source);
  browser->window()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowSettings(browser);
  browser->window()->Show();
}

void OpenSyncSetupWindow(Profile* profile, SyncPromoUI::Source source) {
  Browser* browser = Browser::Create(profile);
  ShowSyncSetup(browser, source);
  browser->window()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowClearBrowsingDataDialog(browser);
  browser->window()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowImportDialog(browser);
  browser->window()->Show();
}

void OpenInstantConfirmDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowInstantConfirmDialog(browser);
  browser->window()->Show();
}

void OpenBookmarkManagerWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowBookmarkManager(browser);
  browser->window()->Show();
}

void OpenExtensionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  ShowExtensions(browser);
  browser->window()->Show();
}

}  // namespace chrome
