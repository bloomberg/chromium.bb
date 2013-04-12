// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/mini_installer_test/installer_test_util.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/test/mini_installer_test/mini_installer_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using installer::InstallationValidator;

namespace {

BrowserDistribution::Type ToBrowserDistributionType(
    InstallationValidator::InstallationType type) {
  const int kChromeMask =
      (InstallationValidator::ProductBits::CHROME_SINGLE |
       InstallationValidator::ProductBits::CHROME_MULTI);
  const int kChromeFrameMask =
      (InstallationValidator::ProductBits::CHROME_FRAME_SINGLE |
       InstallationValidator::ProductBits::CHROME_FRAME_MULTI |
       InstallationValidator::ProductBits::CHROME_FRAME_READY_MODE);
  const int kBinariesMask =
      (InstallationValidator::ProductBits::CHROME_MULTI |
       InstallationValidator::ProductBits::CHROME_FRAME_MULTI |
       InstallationValidator::ProductBits::CHROME_FRAME_READY_MODE);
  // Default return is CHROME_BINARIES.
  BrowserDistribution::Type ret_value = BrowserDistribution::CHROME_BINARIES;
  if (type & kChromeMask)
    ret_value = BrowserDistribution::CHROME_BROWSER;
  if (type & kChromeFrameMask)
    ret_value = BrowserDistribution::CHROME_FRAME;
  if (type & kBinariesMask)
    ret_value = BrowserDistribution::CHROME_BINARIES;
  return ret_value;
}

}  // namespace

namespace installer_test {

bool DeleteInstallDirectory(bool system_level,
                            InstallationValidator::InstallationType type) {
  std::string version = GetVersion(type);
  if (version.empty())
    return false;
  base::FilePath path;
  bool has_install_dir = GetInstallDirectory(system_level,
                                             ToBrowserDistributionType(type),
                                             &path);
  if (!has_install_dir || !file_util::PathExists(path))
    return false;
  path = path.AppendASCII(version);
  return file_util::Delete(path, true);
}

bool DeleteRegistryKey(bool system_level,
                       InstallationValidator::InstallationType type) {
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      ToBrowserDistributionType(type));
  base::FilePath::StringType key(google_update::kRegPathClients);
  key.push_back(base::FilePath::kSeparators[0]);
  key.append(dist->GetAppGuid());
  HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  return InstallUtil::DeleteRegistryKey(root, key);
}

bool GetChromeInstallDirectory(bool system_level, base::FilePath* path) {
  return GetInstallDirectory(system_level,
      BrowserDistribution::CHROME_BROWSER, path);
}

bool GetInstallDirectory(bool system_level,
                         BrowserDistribution::Type type, base::FilePath* path) {
  BrowserDistribution* dist =
      BrowserDistribution::GetSpecificDistribution(type);
  *path = installer::GetChromeInstallPath(system_level, dist);
  base::FilePath parent;
  if (system_level)
    PathService::Get(base::DIR_PROGRAM_FILES, &parent);
  else
    PathService::Get(base::DIR_LOCAL_APP_DATA, &parent);
  return parent.IsParent(*path);
}

bool GetInstalledProducts(
    std::vector<installer_test::InstalledProduct>* products) {
  // Clear out the products list.
  products->clear();
  // Check user-level and system-level for products.
  BrowserDistribution* current_dist;
  installer_test::InstalledProduct current_prod;
  for (int i = 0; i < 2; ++i) {
    const bool system_level = (i != 0);
    InstallationValidator::InstallationType type =
        InstallationValidator::NO_PRODUCTS;
    bool is_valid =
        InstallationValidator::ValidateInstallationType(system_level, &type);
    if (type != InstallationValidator::NO_PRODUCTS) {
      current_dist = BrowserDistribution::GetSpecificDistribution(
          ToBrowserDistributionType(type));
      Version version;
      InstallUtil::GetChromeVersion(current_dist, system_level, &version);
      if (version.IsValid()) {
        current_prod.type = type;
        current_prod.version = version.GetString();
        current_prod.system = system_level;
        products->push_back(current_prod);
      }
    }
  }
  return !products->empty();
}

bool ValidateInstall(bool system_level,
                     InstallationValidator::InstallationType expected,
                     const std::string& version) {
  if (GetVersion(expected) != version)
    return false;
  InstallationValidator::InstallationType type;
  InstallationValidator::ValidateInstallationType(system_level, &type);
  if (type == InstallationValidator::NO_PRODUCTS) {
    LOG(ERROR) << "No installed Chrome or Chrome Frame versions found.";
    return false;
  }
  if ((type & expected) == 0) {
    LOG(ERROR) << "Expected type: " << expected << "\n Actual type: " << type;
    return false;
  }
  return true;
}

std::string GetVersion(InstallationValidator::InstallationType product) {
  std::vector<installer_test::InstalledProduct> installed;
  if (GetInstalledProducts(&installed)) {
    for (size_t i = 0; i < installed.size(); ++i) {
      if ((installed[i].type & product) != 0) {
        return installed[i].version;
      }
    }
  }
  return "";
}

bool Install(const base::FilePath& installer) {
  if (!file_util::PathExists(installer)) {
    LOG(ERROR) << "Installer does not exist: " << installer.MaybeAsASCII();
    return false;
  }
  CommandLine command(installer);
  LOG(INFO) << "Running installer command: "
            << command.GetCommandLineString();
  return installer_test::RunAndWaitForCommandToFinish(command);
}

bool Install(const base::FilePath& installer, const SwitchBuilder& switches) {
  if (!file_util::PathExists(installer)) {
    LOG(ERROR) << "Installer does not exist: " << installer.MaybeAsASCII();
    return false;
  }
  CommandLine command(installer);
  command.AppendArguments(switches.GetSwitches(), false);
  LOG(INFO) << "Running installer command: "
            << command.GetCommandLineString();
  return installer_test::RunAndWaitForCommandToFinish(command);
}

bool LaunchChrome(bool close_after_launch, bool system_level) {
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  base::FilePath install_path;
  if (!GetChromeInstallDirectory(
      system_level, &install_path)) {
    LOG(ERROR) << "Could not find Chrome install directory";
    return false;
  }
  install_path = install_path.Append(installer::kChromeExe);
  CommandLine browser(install_path);

  base::FilePath exe = browser.GetProgram();
  LOG(INFO) << "Browser launch command: " << browser.GetCommandLineString();
  base::ProcessHandle chrome;
  if (!base::LaunchProcess(browser, base::LaunchOptions(), &chrome)) {
    LOG(ERROR) << "Could not launch process: " << exe.value();
    return false;
  }
  if (close_after_launch) {
    if (base::KillProcess(chrome, 0, true)) {
      LOG(ERROR) << "Failed to close chrome.exe after launch.";
      return false;
    }
  }
  return true;
}

bool LaunchIE(const std::string& url) {
  base::FilePath browser_path;
  PathService::Get(base::DIR_PROGRAM_FILES, &browser_path);
  browser_path = browser_path.Append(mini_installer_constants::kIELocation);
  browser_path = browser_path.Append(mini_installer_constants::kIEProcessName);

  CommandLine cmd_line(browser_path);
  cmd_line.AppendArg(url);
  return base::LaunchProcess(cmd_line, base::LaunchOptions(), NULL);
}

bool UninstallAll() {
  base::CleanupProcesses(installer::kChromeExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  base::CleanupProcesses(installer::kChromeFrameHelperExe, base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  std::vector<installer_test::InstalledProduct> installed;
  if (!GetInstalledProducts(&installed)) {
    LOG(WARNING) << "No installed products to uninstall.";
    return false;
  }
  bool ret_val = false;
  for (size_t i = 0; i < installed.size(); ++i) {
    if (!Uninstall(installed[i].system, installed[i].type))
      ret_val = false;
  }
  return ret_val;
}

bool Uninstall(bool system_level,
               InstallationValidator::InstallationType type) {
  std::vector<BrowserDistribution::Type> products;
  if (ToBrowserDistributionType(type) !=
      BrowserDistribution::CHROME_BINARIES) {
    products.push_back(ToBrowserDistributionType(type));
  } else {
    products.push_back(BrowserDistribution::CHROME_BROWSER);
    products.push_back(BrowserDistribution::CHROME_FRAME);
  }
  bool ret_val = false;
  for (size_t i = 0; i < products.size(); ++i) {
    if (!Uninstall(system_level, products[i]))
      ret_val = false;
  }
  return ret_val;
}

bool Uninstall(bool system_level,
               BrowserDistribution::Type product) {
  static const int kMultiMask =
      (InstallationValidator::ProductBits::CHROME_MULTI |
       InstallationValidator::ProductBits::CHROME_FRAME_MULTI);
  CommandLine uninstall_cmd(InstallUtil::GetChromeUninstallCmd(system_level,
      product));
  CommandLine::StringType archive =
      uninstall_cmd.GetProgram().DirName().AppendASCII("chrome.7z").value();
  uninstall_cmd.AppendSwitch(installer::switches::kUninstall);
  uninstall_cmd.AppendSwitch(installer::switches::kForceUninstall);
  uninstall_cmd.AppendSwitchNative(
      installer::switches::kInstallArchive, archive);
  if (system_level)
    uninstall_cmd.AppendSwitch(installer::switches::kSystemLevel);
  if ((product & kMultiMask) !=0)
    uninstall_cmd.AppendSwitch(installer::switches::kMultiInstall);
  LOG(INFO) << "Uninstall command: " << uninstall_cmd.GetCommandLineString();
  bool ret_val = RunAndWaitForCommandToFinish(uninstall_cmd);
  // Close IE notification when uninstalling Chrome Frame.
  base::CleanupProcesses(mini_installer_constants::kIEProcessName,
                         base::TimeDelta(),
                         content::RESULT_CODE_HUNG, NULL);
  return ret_val;
}


bool RunAndWaitForCommandToFinish(CommandLine command) {
  if (!file_util::PathExists(command.GetProgram())) {
    LOG(ERROR) << "Command executable does not exist: "
               << command.GetProgram().MaybeAsASCII();
    return false;
  }
  base::ProcessHandle process;
  if (!base::LaunchProcess(command, base::LaunchOptions(), &process)) {
    LOG(ERROR) << "Failed to launch command: "
               << command.GetCommandLineString();
    return false;
  }
  if (!base::WaitForSingleProcess(process, base::TimeDelta::FromMinutes(1))) {
    LOG(ERROR) << "Launched process did not complete.";
    return false;
  }
  return true;
}

}  // namespace
