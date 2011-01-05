// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a specific implementation of BrowserDistribution class for
// Chrome Frame. It overrides the bare minimum of methods necessary to get a
// Chrome Frame installer that does not interact with Google Chrome or
// Chromium installations.

#include "chrome/installer/util/chrome_frame_distribution.h"

#include <string>

#include "base/string_util.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"

#include "installer_util_strings.h"  // NOLINT

namespace {
const wchar_t kChromeFrameGuid[] = L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
}

ChromeFrameDistribution::ChromeFrameDistribution(
    const installer::MasterPreferences& prefs)
        : BrowserDistribution(prefs), ceee_(prefs.install_ceee()),
          ready_mode_(false) {
  type_ = BrowserDistribution::CHROME_FRAME;
  prefs.GetBool(installer::master_preferences::kChromeFrameReadyMode,
                &ready_mode_);

  bool system_install = false;
  prefs.GetBool(installer::master_preferences::kSystemLevel, &system_install);

  // See if Chrome Frame is already installed.  If so, we must make sure that
  // the ceee and ready mode flags match.
  CommandLine uninstall(CommandLine::NO_PROGRAM);
  if (installer::GetUninstallSwitches(system_install, this, &uninstall)) {
    if (!ceee_ && uninstall.HasSwitch(installer::switches::kCeee)) {
      LOG(INFO) << "CEEE is not specified on the command line but CEEE is "
                   "already installed. Implicitly enabling CEEE.";
      ceee_ = true;
    }

    // If the user has already opted in to CF, we shouldn't set the ready-mode
    // flag.  If we don't do this, we might have two entries in the Add/Remove
    // Programs list that can uninstall GCF.  Also, we can only enable
    // ready-mode if Chrome is also being installed.  Without it, there's no way
    // to uninstall Chrome Frame.
    if (ready_mode_) {
      if (!uninstall.HasSwitch(installer::switches::kChromeFrameReadyMode)) {
        LOG(INFO) << "Ready mode was specified on the command line but GCF "
                     "is already fully installed.  Ignoring command line.";
        ready_mode_ = false;
      } else if (!prefs.install_chrome()) {
        LOG(WARNING) << "Cannot enable ready mode without installing Chrome.";
        ready_mode_ = false;
      }
    }
  }
}

std::wstring ChromeFrameDistribution::GetAppGuid() {
  return kChromeFrameGuid;
}

std::wstring ChromeFrameDistribution::GetApplicationName() {
  const std::wstring& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_FRAME_NAME_BASE);
  return product_name;
}

std::wstring ChromeFrameDistribution::GetAlternateApplicationName() {
  const std::wstring& product_name =
      installer::GetLocalizedString(IDS_PRODUCT_FRAME_NAME_BASE);
  return product_name;
}

std::wstring ChromeFrameDistribution::GetInstallSubDir() {
  return L"Google\\Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetPublisherName() {
  const std::wstring& publisher_name =
      installer::GetLocalizedString(IDS_ABOUT_VERSION_COMPANY_NAME_BASE);
  return publisher_name;
}

std::wstring ChromeFrameDistribution::GetAppDescription() {
  return L"Chrome in a Frame.";
}

std::wstring ChromeFrameDistribution::GetLongAppDescription() {
  return L"Chrome in a Frame.";
}

std::string ChromeFrameDistribution::GetSafeBrowsingName() {
  return "googlechromeframe";
}

std::wstring ChromeFrameDistribution::GetStateKey() {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

std::wstring ChromeFrameDistribution::GetStateMediumKey() {
  std::wstring key(google_update::kRegPathClientStateMedium);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

std::wstring ChromeFrameDistribution::GetStatsServerURL() {
  return L"https://clients4.google.com/firefox/metrics/collect";
}

std::wstring ChromeFrameDistribution::GetUninstallLinkName() {
  return L"Uninstall Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
         L"Google Chrome Frame";
}

std::wstring ChromeFrameDistribution::GetVersionKey() {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

bool ChromeFrameDistribution::CanSetAsDefault() {
  return false;
}

void ChromeFrameDistribution::UpdateInstallStatus(bool system_install,
    bool incremental_install, bool multi_install,
    installer::InstallStatus install_status) {
#if defined(GOOGLE_CHROME_BUILD)
  GoogleUpdateSettings::UpdateInstallStatus(system_install,
      incremental_install, multi_install,
      InstallUtil::GetInstallReturnCode(install_status), kChromeFrameGuid);
#endif
}

std::vector<FilePath> ChromeFrameDistribution::GetKeyFiles() {
  std::vector<FilePath> key_files;
  key_files.push_back(FilePath(installer::kChromeFrameDll));
  if (ceee_) {
    key_files.push_back(FilePath(installer::kCeeeIeDll));
    key_files.push_back(FilePath(installer::kCeeeBrokerExe));
  }
  return key_files;
}

std::vector<FilePath> ChromeFrameDistribution::GetComDllList() {
  std::vector<FilePath> dll_list;
  dll_list.push_back(FilePath(installer::kChromeFrameDll));
  if (ceee_) {
    dll_list.push_back(FilePath(installer::kCeeeInstallHelperDll));
    dll_list.push_back(FilePath(installer::kCeeeIeDll));
  }
  return dll_list;
}

void ChromeFrameDistribution::AppendUninstallCommandLineFlags(
    CommandLine* cmd_line) {
  DCHECK(cmd_line);
  cmd_line->AppendSwitch(installer::switches::kChromeFrame);

  if (ceee_)
    cmd_line->AppendSwitch(installer::switches::kCeee);

  if (ready_mode_)
    cmd_line->AppendSwitch(installer::switches::kChromeFrameReadyMode);
}

bool ChromeFrameDistribution::ShouldCreateUninstallEntry() {
  // If Chrome Frame is being installed in ready mode, then we will not
  // add an entry to the add/remove dialog.
  return !ready_mode_;
}

bool ChromeFrameDistribution::SetChannelFlags(
    bool set,
    installer::ChannelInfo* channel_info) {
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(channel_info);
  bool modified = channel_info->SetChromeFrame(set);

  // Always remove the options if we're called to remove flags.
  if (!set || ceee_)
    modified |= channel_info->SetCeee(set);

  if (!set || ready_mode_)
    modified |= channel_info->SetReadyMode(set);

  return modified;
#else
  return false;
#endif
}
