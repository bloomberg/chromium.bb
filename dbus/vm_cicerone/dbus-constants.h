// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_VM_CICERONE_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_VM_CICERONE_DBUS_CONSTANTS_H_

namespace vm_tools {
namespace cicerone {

const char kVmCiceroneInterface[] = "org.chromium.VmCicerone";
const char kVmCiceroneServicePath[] = "/org/chromium/VmCicerone";
const char kVmCiceroneServiceName[] = "org.chromium.VmCicerone";

// Methods to be called from vm_concierge.
const char kNotifyVmStartedMethod[] = "NotifyVmStarted";
const char kNotifyVmStoppedMethod[] = "NotifyVmStopped";
const char kGetContainerTokenMethod[] = "GetContainerToken";

// Methods to be called from Chrome.
const char kLaunchContainerApplicationMethod[] = "LaunchContainerApplication";
const char kGetContainerAppIconMethod[] = "GetContainerAppIcon";
const char kLaunchVshdMethod[] = "LaunchVshd";
const char kGetLinuxPackageInfoMethod[] = "GetLinuxPackageInfo";
const char kInstallLinuxPackageMethod[] = "InstallLinuxPackage";
const char kUninstallPackageOwningFileMethod[] = "UninstallPackageOwningFile";
const char kCreateLxdContainerMethod[] = "CreateLxdContainer";
const char kDeleteLxdContainerMethod[] = "DeleteLxdContainer";
const char kStartLxdContainerMethod[] = "StartLxdContainer";
const char kSetTimezoneMethod[] = "SetTimezone";
const char kGetLxdContainerUsernameMethod[] = "GetLxdContainerUsername";
const char kSetUpLxdContainerUserMethod[] = "SetUpLxdContainerUser";
const char kExportLxdContainerMethod[] = "ExportLxdContainer";
const char kImportLxdContainerMethod[] = "ImportLxdContainer";
const char kCancelExportLxdContainerMethod[] = "CancelExportLxdContainer";
const char kCancelImportLxdContainerMethod[] = "CancelImportLxdContainer";
const char kApplyAnsiblePlaybookMethod[] = "ApplyAnsiblePlaybook";
const char kUpgradeContainerMethod[] = "UpgradeContainer";
const char kCancelUpgradeContainerMethod[] = "CancelUpgradeContainer";
const char kConfigureForArcSideloadMethod[] = "ConfigureForArcSideload";
const char kStartLxdMethod[] = "StartLxd";

// Methods to be called from chunneld.
const char kConnectChunnelMethod[] = "ConnectChunnel";

// Methods to be called from debugd.
const char kGetDebugInformationMethod[] = "GetDebugInformation";

// Signals.
const char kContainerStartedSignal[] = "ContainerStarted";
const char kContainerShutdownSignal[] = "ContainerShutdown";
const char kInstallLinuxPackageProgressSignal[] = "InstallLinuxPackageProgress";
const char kUninstallPackageProgressSignal[] = "UninstallPackageProgress";
const char kLxdContainerCreatedSignal[] = "LxdContainerCreated";
const char kLxdContainerDeletedSignal[] = "LxdContainerDeleted";
const char kLxdContainerDownloadingSignal[] = "LxdContainerDownloading";
const char kLxdContainerStartingSignal[] = "LxdContainerStarting";
const char kTremplinStartedSignal[] = "TremplinStarted";
const char kExportLxdContainerProgressSignal[] = "ExportLxdContainerProgress";
const char kImportLxdContainerProgressSignal[] = "ImportLxdContainerProgress";
const char kPendingAppListUpdatesSignal[] = "PendingAppListUpdates";
const char kApplyAnsiblePlaybookProgressSignal[] =
    "ApplyAnsiblePlaybookProgress";
const char kUpgradeContainerProgressSignal[] = "UpgradeContainerProgress";
const char kStartLxdProgressSignal[] = "StartLxdProgress";

}  // namespace cicerone
}  // namespace vm_tools

#endif  // SYSTEM_API_DBUS_VM_CICERONE_DBUS_CONSTANTS_H_
