// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_settings_test.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/web_contents.h"

ManagedUserSettingsTest::ManagedUserSettingsTest() {
}

ManagedUserSettingsTest::~ManagedUserSettingsTest() {
}

void ManagedUserSettingsTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
  // Set up the ManagedModeNavigationObserver manually since the profile was
  // not managed when the browser was created.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ManagedModeNavigationObserver::CreateForWebContents(web_contents);

  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(browser()->profile());
  managed_user_service->InitForTesting();
}

void ManagedUserSettingsTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableManagedUsers);
}
