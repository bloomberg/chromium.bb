// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_CECSERVICE_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_CECSERVICE_DBUS_CONSTANTS_H_

namespace cecservice {
const char kCecServiceInterface[] = "org.chromium.CecService";
const char kCecServicePath[] = "/org/chromium/CecService";
const char kCecServiceName[] = "org.chromium.CecService";

// Methods.
const char kSendStandByToAllDevicesMethod[] = "SendStandByToAllDevices";
const char kSendWakeUpToAllDevicesMethod[] = "SendWakeUpToAllDevices";

}  // namespace cecservice

#endif  // SYSTEM_API_DBUS_CECSERVICE_DBUS_CONSTANTS_H_
