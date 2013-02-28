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
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowAboutChrome(browser);
  browser->window()->Show();
}

void OpenHistoryWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowHistory(browser);
  browser->window()->Show();
}

void OpenDownloadsWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowDownloads(browser);
  browser->window()->Show();
}

void OpenHelpWindow(Profile* profile, HelpSource source) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowHelp(browser, source);
  browser->window()->Show();
}

void OpenOptionsWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowSettings(browser);
  browser->window()->Show();
}

void OpenSyncSetupWindow(Profile* profile, SyncPromoUI::Source source) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowBrowserSignin(browser, source);
  browser->window()->Show();
}

void OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowClearBrowsingDataDialog(browser);
  browser->window()->Show();
}

void OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowImportDialog(browser);
  browser->window()->Show();
}

void OpenBookmarkManagerWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowBookmarkManager(browser);
  browser->window()->Show();
}

void OpenExtensionsWindow(Profile* profile) {
  Browser* browser =
      new Browser(Browser::CreateParams(profile,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE));
  ShowExtensions(browser, std::string());
  browser->window()->Show();
}

}  // namespace chrome
