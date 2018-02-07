// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_VM_CONCIERGE_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_VM_CONCIERGE_DBUS_CONSTANTS_H_

namespace vm_tools {
namespace concierge {

const char kVmConciergeInterface[] = "org.chromium.VmConcierge";
const char kVmConciergeServicePath[] = "/org/chromium/VmConcierge";
const char kVmConciergeServiceName[] = "org.chromium.VmConcierge";

const char kStartVmMethod[] = "StartVm";
const char kStopVmMethod[] = "StopVm";
const char kStopAllVmsMethod[] = "StopAllVms";
const char kGetVmInfoMethod[] = "GetVmInfo";
const char kCreateDiskImageMethod[] = "CreateDiskImage";

}  // namespace concierge
}  // namespace vm_tools


#endif  // SYSTEM_API_DBUS_VM_CONCIERGE_DBUS_CONSTANTS_H_
