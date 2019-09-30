// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_U2F_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_U2F_DBUS_CONSTANTS_H_

namespace u2f {

// Interface exposed by the u2f daemon.

const char kU2FInterface[] = "org.chromium.U2F";
const char kU2FServicePath[] = "/org/chromium/U2F";
const char kU2FServiceName[] = "org.chromium.U2F";

// Signals of the u2f interface:
const char kU2FUserNotificationSignal[] = "UserNotification";

// Methods of the u2f interface:
const char kU2FMakeCredential[] = "MakeCredential";
const char kU2FGetAssertion[] = "GetAssertion";
const char kU2FHasCredentials[] = "HasCredentials";

}  // namespace u2f

#endif  // SYSTEM_API_DBUS_U2F_DBUS_CONSTANTS_H_
