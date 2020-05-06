// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_

namespace dlcservice {

constexpr char kDlcServiceInterface[] = "org.chromium.DlcServiceInterface";
constexpr char kDlcServiceServicePath[] = "/org/chromium/DlcService";
constexpr char kDlcServiceServiceName[] = "org.chromium.DlcService";

constexpr char kGetInstalledMethod[] = "GetInstalled";
constexpr char kInstallMethod[] = "Install";
constexpr char kInstallDlcMethod[] = "InstallDlc";
constexpr char kUninstallMethod[] = "Uninstall";
constexpr char kPurgeMethod[] = "Purge";
constexpr char kGetExistingDlcsMethod[] = "GetExistingDlcs";
constexpr char kOnInstallStatusSignal[] = "OnInstallStatus";

// Error Codes from dlcservice.
constexpr char kErrorNone[] = "org.chromium.DlcServiceInterface.NONE";
constexpr char kErrorInternal[] = "org.chromium.DlcServiceInterface.INTERNAL";
constexpr char kErrorBusy[] = "org.chromium.DlcServiceInterface.BUSY";
constexpr char kErrorNeedReboot[] =
    "org.chromium.DlcServiceInterface.NEED_REBOOT";
constexpr char kErrorInvalidDlc[] =
    "org.chromium.DlcServiceInterface.INVALID_DLC";
constexpr char kErrorAllocation[] =
    "org.chromium.DlcServiceInterface.ALLOCATION";

}  // namespace dlcservice

#endif  // SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_
