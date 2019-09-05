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
constexpr char kUninstallMethod[] = "Uninstall";
constexpr char kOnInstallStatusSignal[] = "OnInstallStatus";

// Error Codes from dlcservice.
constexpr char kErrorNone[] = "NONE";
constexpr char kErrorInternal[] = "INTERNAL";
constexpr char kErrorBusy[] = "BUSY";
constexpr char kErrorNeedReboot[] = "NEED_REBOOT";
constexpr char kErrorInvalidDlc[] = "INVALID_DLC";

}  // namespace dlcservice

#endif  // SYSTEM_API_DBUS_DLCSERVICE_DBUS_CONSTANTS_H_
