// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/product.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/win/registry.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/package_properties.h"

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

////////////////////////////////////////////////////////////////////////////////

Product::Product(BrowserDistribution* distribution, Package* package)
    : distribution_(distribution),
      package_(package),
      msi_(false),
      cache_state_(0) {
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
  if ((cache_state_ & MSI_STATE) == 0) {
    msi_ = false;  // Covers failure cases below.

    const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();

    bool is_msi = false;
    prefs.GetBool(installer::master_preferences::kMsi, &is_msi);

    if (!is_msi) {
      // We didn't find it in the preferences, try looking in the registry.
      HKEY reg_root = system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
      RegKey key;
      if (key.Open(reg_root, distribution_->GetStateKey().c_str(), KEY_READ)) {
        DWORD msi_value;
        if (key.ReadValueDW(google_update::kRegMSIField, &msi_value) &&
            msi_value != 0) {
          msi_ = true;
        }
      }
    } else {
      msi_ = true;
    }
    cache_state_ |= MSI_STATE;
  }

  return msi_;
}

bool Product::SetMsiMarker(bool set) const {
  bool success = false;
  HKEY reg_root = system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
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

const Version* Product::GetInstalledVersion() const {
  if ((cache_state_ & VERSION) == 0) {
    DCHECK(installed_version_.get() == NULL);
    installed_version_.reset(InstallUtil::GetChromeVersion(distribution_,
                                                           system_level()));
    cache_state_ |= VERSION;
  }

  return installed_version_.get();
}

bool Product::IsInstalled() const {
  return GetInstalledVersion() != NULL;
}

bool Product::ShouldCreateUninstallEntry() const {
  if (IsMsi()) {
    // MSI installations will manage their own uninstall shortcuts.
    return false;
  }

  return distribution_->ShouldCreateUninstallEntry();
}

///////////////////////////////////////////////////////////////////////////////
ProductPackageMapping::ProductPackageMapping(bool multi_install,
                                             bool system_level)
#if defined(GOOGLE_CHROME_BUILD)
    : package_properties_(new ChromePackageProperties()),
#else
    : package_properties_(new ChromiumPackageProperties()),
#endif
      multi_install_(multi_install),
      system_level_(system_level) {
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

  FilePath install_package;
  if (distribution->GetType() == BrowserDistribution::CHROME_BROWSER) {
    install_package = GetChromeInstallPath(system_level_, distribution);
  } else {
    DCHECK_EQ(BrowserDistribution::CHROME_FRAME, distribution->GetType());
    install_package = GetChromeFrameInstallPath(multi_install_, system_level_,
                                                distribution);
  }

  if (install_package.empty()) {
    LOG(ERROR) << "Got an empty installation path for "
               << distribution->GetApplicationName()
               << ".  It's likely that there's a conflicting "
                  "installation present";
    return false;
  }

  scoped_refptr<Package> target_package;
  for (size_t i = 0; i < packages_.size(); ++i) {
    if (packages_[i]->IsEqual(install_package)) {
      // Use an existing Package.
      target_package = packages_[i];
      break;
    }
  }

  if (!target_package.get()) {
    DCHECK(packages_.empty()) << "Multiple packages per run unsupported.";
    // create new one and add.
    target_package = new Package(multi_install_, system_level_, install_package,
                                 package_properties_.get());
    packages_.push_back(target_package);
  }

  scoped_refptr<Product> product(new Product(distribution, target_package));
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
    const MasterPreferences& prefs) {
  BrowserDistribution* distribution =
      BrowserDistribution::GetSpecificDistribution(type, prefs);
  if (!distribution) {
    NOTREACHED();
    return false;
  }

  return AddDistribution(distribution);
}

}  // namespace installer

