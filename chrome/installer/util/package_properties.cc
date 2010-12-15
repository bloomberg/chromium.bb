// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/package_properties.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"

using installer::InstallStatus;

namespace {
// TODO(tommi): Use google_update::kMultiInstallAppGuid
const wchar_t kChromePackageGuid[] =
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

std::wstring GetStateKeyForGuid(const wchar_t* guid) {
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(guid);
  return key;
}

std::wstring GetVersionKeyForGuid(const wchar_t* guid) {
  std::wstring key(google_update::kRegPathClients);
  key.append(L"\\");
  key.append(guid);
  return key;
}

}  // end namespace

namespace installer {

const char PackageProperties::kPackageProductName[] = "Chrome binaries";

PackagePropertiesImpl::PackagePropertiesImpl(const wchar_t* guid,
                                             const std::wstring& state_key,
                                             const std::wstring& version_key)
    : guid_(guid), state_key_(state_key), version_key_(version_key) {
}

PackagePropertiesImpl::~PackagePropertiesImpl() {
}

const std::wstring& PackagePropertiesImpl::GetStateKey() {
  return state_key_;
}

const std::wstring& PackagePropertiesImpl::GetVersionKey() {
  return version_key_;
}

void PackagePropertiesImpl::UpdateDiffInstallStatus(bool system_level,
                                                    bool incremental_install,
                                                    InstallStatus status) {
  GoogleUpdateSettings::UpdateDiffInstallStatus(system_level,
      incremental_install, BrowserDistribution::GetInstallReturnCode(status),
      guid_);
}

ChromiumPackageProperties::ChromiumPackageProperties()
    : PackagePropertiesImpl(L"", L"Software\\Chromium", L"Software\\Chromium") {
}

ChromiumPackageProperties::~ChromiumPackageProperties() {
}

ChromePackageProperties::ChromePackageProperties()
    : PackagePropertiesImpl(kChromePackageGuid,
                            GetStateKeyForGuid(kChromePackageGuid),
                            GetVersionKeyForGuid(kChromePackageGuid)) {
}

ChromePackageProperties::~ChromePackageProperties() {
}

}  // namespace installer
