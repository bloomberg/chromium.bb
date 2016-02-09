// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_mac.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"

namespace chrome {

void OpenAboutWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowAboutChrome(browser);
  browser->window()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile));
  ShowHistory(browser);
  browser->window()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowDownloads(browser);
  browser->window()->Show();
}

void OpenHelpWindow(Profile* profile, HelpSource source) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowHelp(browser, source);
  browser->window()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowSettings(browser);
  browser->window()->Show();
}

void OpenSyncSetupWindow(Profile* profile,
                         signin_metrics::AccessPoint access_point) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowBrowserSigninOrSettings(browser, access_point);
  browser->window()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowClearBrowsingDataDialog(browser);
  browser->window()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowImportDialog(browser);
  browser->window()->Show();
}

void OpenBookmarkManagerWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowBookmarkManager(browser);
  browser->window()->Show();
}

void OpenExtensionsWindow(Profile* profile) {
  Browser* browser = new Browser(Browser::CreateParams(profile));
  ShowExtensions(browser, std::string());
  browser->window()->Show();
}

}  // namespace chrome
