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
const char kIsContainerRunningMethod[] = "IsContainerRunning";

// Methods to be called from Chrome.
const char kLaunchContainerApplicationMethod[] = "LaunchContainerApplication";
const char kGetContainerAppIconMethod[] = "GetContainerAppIcon";
const char kLaunchVshdMethod[] = "LaunchVshd";
const char kGetLinuxPackageInfoMethod[] = "GetLinuxPackageInfo";
const char kInstallLinuxPackageMethod[] = "InstallLinuxPackage";

// Signals.
const char kContainerStartedSignal[] = "ContainerStarted";
const char kContainerShutdownSignal[] = "ContainerShutdown";
const char kInstallLinuxPackageProgressSignal[] =
    "InstallLinuxPackageProgress";

}  // namespace cicerone
}  // namespace vm_tools


#endif  // SYSTEM_API_DBUS_VM_CICERONE_DBUS_CONSTANTS_H_
