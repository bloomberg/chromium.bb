// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_PACKAGE_PROPERTIES_H_
#define CHROME_INSTALLER_UTIL_PACKAGE_PROPERTIES_H_
#pragma once

#include <windows.h>

#include <string>

#include "base/basictypes.h"

namespace installer {
enum InstallStatus;
};

namespace installer {

// Pure virtual interface that exposes properties of a package installation.
// A package represents a set of binaries on disk that can be shared by two or
// more products.  Also see the Package class for further details.
// PackageProperties is comparable to the BrowserDistribution class but the
// difference is that the BrowserDistribution class represents a product
// installation whereas PackageProperties represents a package
// (horizontal vs vertical).
class PackageProperties {
 public:
  PackageProperties() {}
  virtual ~PackageProperties() {}

  static const char kPackageProductName[];

  // Returns true iff this package will be updated by Google Update.
  virtual bool ReceivesUpdates() const = 0;

  // Equivalent to BrowserDistribution::GetAppGuid()
  virtual const std::wstring& GetAppGuid() = 0;
  virtual const std::wstring& GetStateKey() = 0;
  virtual const std::wstring& GetStateMediumKey() = 0;
  virtual const std::wstring& GetVersionKey() = 0;
  virtual void UpdateInstallStatus(bool system_level, bool incremental_install,
      bool multi_install, installer::InstallStatus status) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PackageProperties);
};  // class PackageProperties

class PackagePropertiesImpl : public PackageProperties {
 public:
  explicit PackagePropertiesImpl(const wchar_t* guid,
                                 const std::wstring& state_key,
                                 const std::wstring& state_medium_key,
                                 const std::wstring& version_key);
  virtual ~PackagePropertiesImpl();

  virtual const std::wstring& GetAppGuid();
  virtual const std::wstring& GetStateKey();
  virtual const std::wstring& GetStateMediumKey();
  virtual const std::wstring& GetVersionKey();
  virtual void UpdateInstallStatus(bool system_level, bool incremental_install,
      bool multi_install, installer::InstallStatus status);

 protected:
  std::wstring guid_;
  std::wstring state_key_;
  std::wstring state_medium_key_;
  std::wstring version_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PackagePropertiesImpl);
};  // class PackagePropertiesImpl

class ChromiumPackageProperties : public PackagePropertiesImpl {
 public:
  ChromiumPackageProperties();
  virtual ~ChromiumPackageProperties();

  virtual bool ReceivesUpdates() const {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumPackageProperties);
};  // class ChromiumPackageProperties

class ChromePackageProperties : public PackagePropertiesImpl {
 public:
  ChromePackageProperties();
  virtual ~ChromePackageProperties();

  virtual bool ReceivesUpdates() const {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromePackageProperties);
};  // class ChromePackageProperties

#if defined(GOOGLE_CHROME_BUILD)
typedef ChromePackageProperties ActivePackageProperties;
#else
typedef ChromiumPackageProperties ActivePackageProperties;
#endif

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_PACKAGE_PROPERTIES_H_
