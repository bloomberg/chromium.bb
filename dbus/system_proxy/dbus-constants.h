// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SYSTEM_API_DBUS_SYSTEM_PROXY_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_SYSTEM_PROXY_DBUS_CONSTANTS_H_

namespace system_proxy {
// General
const char kSystemProxyInterface[] = "org.chromium.SystemProxy";
const char kSystemProxyServicePath[] = "/org/chromium/SystemProxy";
const char kSystemProxyServiceName[] = "org.chromium.SystemProxy";

// Methods
const char kSetSystemTrafficCredentialsMethod[] = "SetSystemTrafficCredentials";
const char kShutDownMethod[] = "ShutDown";

}  // namespace system_proxy
#endif  // SYSTEM_API_DBUS_SYSTEM_PROXY_DBUS_CONSTANTS_H_
