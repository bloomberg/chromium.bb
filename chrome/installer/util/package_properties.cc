// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/package_properties.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"

namespace {

const wchar_t kChromePackageGuid[] =
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

std::wstring GetKeyForGuid(const wchar_t* base_key, const wchar_t* guid) {
  std::wstring key(base_key);
  key.append(L"\\");
  key.append(guid);
  return key;
}

}  // end namespace

namespace installer {

const char PackageProperties::kPackageProductName[] = "Chrome binaries";

PackagePropertiesImpl::PackagePropertiesImpl(
    const wchar_t* guid,
    const std::wstring& state_key,
    const std::wstring& state_medium_key,
    const std::wstring& version_key)
    : guid_(guid), state_key_(state_key), state_medium_key_(state_medium_key),
      version_key_(version_key) {
}

PackagePropertiesImpl::~PackagePropertiesImpl() {
}

const std::wstring& PackagePropertiesImpl::GetStateKey() {
  return state_key_;
}

const std::wstring& PackagePropertiesImpl::GetStateMediumKey() {
  return state_medium_key_;
}

const std::wstring& PackagePropertiesImpl::GetVersionKey() {
  return version_key_;
}

void PackagePropertiesImpl::UpdateInstallStatus(bool system_level,
                                                bool incremental_install,
                                                bool multi_install,
                                                InstallStatus status) {
  if (ReceivesUpdates()) {
    GoogleUpdateSettings::UpdateInstallStatus(system_level,
        incremental_install, multi_install,
        InstallUtil::GetInstallReturnCode(status), guid_);
  }
}

ChromiumPackageProperties::ChromiumPackageProperties()
    : PackagePropertiesImpl(L"", L"Software\\Chromium", L"Software\\Chromium",
                            L"Software\\Chromium") {
}

ChromiumPackageProperties::~ChromiumPackageProperties() {
}

ChromePackageProperties::ChromePackageProperties()
    : PackagePropertiesImpl(
          kChromePackageGuid,
          GetKeyForGuid(google_update::kRegPathClientState,
                        kChromePackageGuid),
          GetKeyForGuid(google_update::kRegPathClientStateMedium,
                        kChromePackageGuid),
          GetKeyForGuid(google_update::kRegPathClients,
                        kChromePackageGuid)) {
}

ChromePackageProperties::~ChromePackageProperties() {
}

}  // namespace installer
