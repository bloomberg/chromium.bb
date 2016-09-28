// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_common.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/accessibility/accessibility_util.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"

using chromeos::LoginState;

namespace {

const char kPaletteSettingsSubPageName[] = "stylus-overlay";

void ShowSettingsSubPageForActiveUser(const std::string& sub_page) {
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        sub_page);
}

}  // namespace

// static
const char SystemTrayCommon::kDisplaySettingsSubPageName[] = "display";
const char SystemTrayCommon::kDisplayOverscanSettingsSubPageName[] =
    "displayOverscan";

// static
void SystemTrayCommon::ShowSettings() {
  ShowSettingsSubPageForActiveUser(std::string());
}

// static
void SystemTrayCommon::ShowDateSettings() {
  content::RecordAction(base::UserMetricsAction("ShowDateOptions"));
  std::string sub_page =
      std::string(chrome::kSearchSubPage) + "#" +
      l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SECTION_TITLE_DATETIME);
  // Everybody can change the time zone (even though it is a device setting).
  chrome::ShowSettingsSubPageForProfile(ProfileManager::GetActiveUserProfile(),
                                        sub_page);
}

// static
void SystemTrayCommon::ShowDisplaySettings() {
  content::RecordAction(base::UserMetricsAction("ShowDisplayOptions"));
  ShowSettingsSubPageForActiveUser(kDisplaySettingsSubPageName);
}

// static
void SystemTrayCommon::ShowChromeSlow() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetPrimaryUserProfile());
  chrome::ShowSlow(displayer.browser());
}

// static
void SystemTrayCommon::ShowIMESettings() {
  content::RecordAction(base::UserMetricsAction("OpenLanguageOptionsDialog"));
  ShowSettingsSubPageForActiveUser(chrome::kLanguageOptionsSubPage);
}

// static
void SystemTrayCommon::ShowHelp() {
  chrome::ShowHelpForProfile(ProfileManager::GetActiveUserProfile(),
                             chrome::HELP_SOURCE_MENU);
}

// static
void SystemTrayCommon::ShowAccessibilityHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chromeos::accessibility::ShowAccessibilityHelp(displayer.browser());
}

// static
void SystemTrayCommon::ShowAccessibilitySettings() {
  content::RecordAction(base::UserMetricsAction("ShowAccessibilitySettings"));
  std::string sub_page = std::string(chrome::kSearchSubPage) + "#" +
                         l10n_util::GetStringUTF8(
                             IDS_OPTIONS_SETTINGS_SECTION_TITLE_ACCESSIBILITY);
  ShowSettingsSubPageForActiveUser(sub_page);
}

// static
void SystemTrayCommon::ShowPaletteHelp() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowSingletonTab(displayer.browser(),
                           GURL(chrome::kChromePaletteHelpURL));
}

// static
void SystemTrayCommon::ShowPaletteSettings() {
  content::RecordAction(base::UserMetricsAction("ShowPaletteOptions"));
  ShowSettingsSubPageForActiveUser(kPaletteSettingsSubPageName);
}

// static
void SystemTrayCommon::ShowPublicAccountInfo() {
  chrome::ScopedTabbedBrowserDisplayer displayer(
      ProfileManager::GetActiveUserProfile());
  chrome::ShowPolicy(displayer.browser());
}

// static
void SystemTrayCommon::ShowProxySettings() {
  LoginState* login_state = LoginState::Get();
  // User is not logged in.
  CHECK(!login_state->IsUserLoggedIn() ||
        login_state->GetLoggedInUserType() == LoginState::LOGGED_IN_USER_NONE);
  chromeos::LoginDisplayHost::default_host()->OpenProxySettings();
}
