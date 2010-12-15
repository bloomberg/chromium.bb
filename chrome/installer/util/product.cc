// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/product.h"

#include <algorithm>

#include "base/logging.h"
#include "base/process_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/work_item_list.h"

using base::win::RegKey;
using installer::MasterPreferences;

namespace {
class ProductIsOfType {
 public:
  explicit ProductIsOfType(BrowserDistribution::Type type)
      : type_(type) { }
  bool operator()(
      const scoped_refptr<const installer::Product>& pi) const {
    return pi->distribution()->GetType() == type_;
  }
 private:
  BrowserDistribution::Type type_;
};  // class ProductIsOfType
}  // end namespace

namespace installer {

const Product* FindProduct(const Products& products,
    BrowserDistribution::Type type) {
  Products::const_iterator i =
      std::find_if(products.begin(), products.end(), ProductIsOfType(type));
  return i == products.end() ? NULL : *i;
}

void WriteInstallerResult(const Products& products,
                          installer::InstallStatus status,
                          int string_resource_id,
                          const std::wstring* const launch_cmd) {
  Products::const_iterator end = products.end();
  for (Products::const_iterator i = products.begin(); i != end; ++i)
    (*i)->WriteInstallerResult(status, string_resource_id, launch_cmd);
}

////////////////////////////////////////////////////////////////////////////////

Product::Product(BrowserDistribution* distribution, bool system_level,
                 Package* package)
    : distribution_(distribution),
      system_level_(system_level),
      package_(package),
      msi_(MSI_NOT_CHECKED) {
  package_->AssociateProduct(this);
}

const Package& Product::package() const {
  return *package_.get();
}

FilePath Product::GetUserDataPath() const {
  return GetChromeUserDataPath(distribution_);
}

bool Product::LaunchChrome() const {
  const FilePath& install_package = package_->path();
  bool success = !install_package.empty();
  if (success) {
    CommandLine cmd(install_package.Append(installer::kChromeExe));
    success = base::LaunchApp(cmd, false, false, NULL);
  }
  return success;
}

bool Product::LaunchChromeAndWait(const CommandLine& options,
                                  int32* exit_code) const {
  const FilePath& install_package = package_->path();
  if (install_package.empty())
    return false;

  CommandLine cmd(install_package.Append(installer::kChromeExe));
  cmd.AppendArguments(options, false);

  bool success = false;
  STARTUPINFOW si = { sizeof(si) };
  PROCESS_INFORMATION pi = {0};
  // Cast away constness of the command_line_string() since CreateProcess
  // might modify the string (insert \0 to separate the program from the
  // arguments).  Since we're not using the cmd variable beyond this point
  // we don't care.
  if (!::CreateProcess(cmd.GetProgram().value().c_str(),
                       const_cast<wchar_t*>(cmd.command_line_string().c_str()),
                       NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                       &si, &pi)) {
    PLOG(ERROR) << "Failed to launch: " << cmd.command_line_string();
  } else {
    ::CloseHandle(pi.hThread);

    DWORD ret = ::WaitForSingleObject(pi.hProcess, INFINITE);
    DLOG_IF(ERROR, ret != WAIT_OBJECT_0)
        << "Unexpected return value from WaitForSingleObject: " << ret;
    if (::GetExitCodeProcess(pi.hProcess, &ret)) {
      DCHECK(ret != STILL_ACTIVE);
      success = true;
      if (exit_code)
        *exit_code = ret;
    } else {
      PLOG(ERROR) << "GetExitCodeProcess failed";
    }

    ::CloseHandle(pi.hProcess);
  }

  return success;
}

bool Product::IsMsi() const {
  if (msi_ == MSI_NOT_CHECKED) {
    msi_ = NOT_MSI;  // Covers failure cases below.

    const MasterPreferences& prefs =
        installer::MasterPreferences::ForCurrentProcess();

    bool is_msi = false;
    prefs.GetBool(installer::master_preferences::kMsi, &is_msi);

    if (!is_msi) {
      // We didn't find it in the preferences, try looking in the registry.
      HKEY reg_root = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
      RegKey key;
      if (key.Open(reg_root, distribution_->GetStateKey().c_str(), KEY_READ)) {
        DWORD msi_value;
        if (key.ReadValueDW(google_update::kRegMSIField, &msi_value)) {
          msi_ = (msi_value == 1) ? IS_MSI : NOT_MSI;
        }
      }
    } else {
      msi_ = IS_MSI;
    }
  }

  return msi_ == IS_MSI;
}

bool Product::SetMsiMarker(bool set) const {
  bool success = false;
  HKEY reg_root = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey client_state_key;
  if (client_state_key.Open(reg_root, distribution_->GetStateKey().c_str(),
                            KEY_READ | KEY_WRITE)) {
    DWORD msi_value = set ? 1 : 0;
    if (client_state_key.WriteValue(google_update::kRegMSIField, msi_value)) {
      success = true;
    } else {
      LOG(ERROR) << "Could not write MSI value to client state key.";
    }
  } else {
    LOG(ERROR) << "SetMsiMarker: Could not open client state key!";
  }

  return success;
}

void Product::WriteInstallerResult(
    installer::InstallStatus status, int string_resource_id,
    const std::wstring* const launch_cmd) const {
  HKEY root = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring key(distribution_->GetStateKey());
  int installer_result = distribution_->GetInstallReturnCode(status) == 0 ?
                         0 : 1;
  scoped_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  install_list->AddCreateRegKeyWorkItem(root, key);
  install_list->AddSetRegValueWorkItem(root, key,
                                       installer::kInstallerResult,
                                       installer_result, true);
  install_list->AddSetRegValueWorkItem(root, key,
                                       installer::kInstallerError,
                                       status, true);
  if (string_resource_id != 0) {
    std::wstring msg = installer::GetLocalizedString(string_resource_id);
    install_list->AddSetRegValueWorkItem(root, key,
        installer::kInstallerResultUIString, msg, true);
  }
  if (launch_cmd != NULL && !launch_cmd->empty()) {
    install_list->AddSetRegValueWorkItem(root, key,
        installer::kInstallerSuccessLaunchCmdLine, *launch_cmd, true);
  }
  if (!install_list->Do())
    LOG(ERROR) << "Failed to record installer error information in registry.";
}

Version* Product::GetInstalledVersion() const {
  return InstallUtil::GetChromeVersion(distribution_, system_level_);
}

///////////////////////////////////////////////////////////////////////////////
ProductPackageMapping::ProductPackageMapping(bool system_level)
    : system_level_(system_level) {
}

const Packages& ProductPackageMapping::packages() const {
  return packages_;
}

const Products& ProductPackageMapping::products() const {
  return products_;
}

bool ProductPackageMapping::AddDistribution(BrowserDistribution* distribution) {
  DCHECK(distribution);
  // Each product type can be added exactly once.
  DCHECK(FindProduct(products_, distribution->GetType()) == NULL);

  FilePath install_package(GetChromeInstallPath(system_level_, distribution));

  scoped_refptr<Package> target_package;
  for (size_t i = 0; i < packages_.size(); ++i) {
    if (packages_[i]->IsEqual(install_package)) {
      // Use an existing Package.
      target_package = packages_[i];
      break;
    }
  }

  if (!target_package.get()) {
    // create new one and add.
    target_package = new Package(install_package);
    packages_.push_back(target_package);
  }

  scoped_refptr<Product> product(
      new Product(distribution, system_level_, target_package));
#ifndef NDEBUG
  for (size_t i = 0; i < products_.size(); ++i) {
    DCHECK_EQ(product->IsMsi(), products_[i]->IsMsi());
  }
#endif
  products_.push_back(product);

  return true;
}

bool ProductPackageMapping::AddDistribution(
    BrowserDistribution::Type type,
    const installer::MasterPreferences& prefs) {
  BrowserDistribution* distribution =
      BrowserDistribution::GetSpecificDistribution(type, prefs);
  if (!distribution) {
    NOTREACHED();
    return false;
  }

  return AddDistribution(distribution);
}

}  // namespace installer

