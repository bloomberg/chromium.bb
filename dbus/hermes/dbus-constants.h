// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_HERMES_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_HERMES_DBUS_CONSTANTS_H_

namespace hermes {

const char kHermesInterface[] = "org.chromium.Hermes";
const char kHermesServicePath[] = "/org/chromium/Hermes";
const char kHermesServiceName[] = "org.chromium.Hermes";

namespace manager {

// Manager methods.
const char kInstallProfileFromActivationCode[] =
    "InstallProfileFromActivationCode";
const char kInstallProfileFromEvent[] = "InstallProfileFromEvent";
const char kUninstallProfile[] = "UninstallProfile";
const char kRequestPendingEvents[] = "RequestPendingEvents";
const char kSetTestMode[] = "SetTestMode";

// Manager properties.
const char kProfilesProperty[] = "Profiles";

}  // namespace manager

namespace profile {

// Profile methods.
const char kEnable[] = "Enable";
const char kDisable[] = "Disable";

// Profile properties.
const char kActivationCodeProperty[] = "ActivationCode";
const char kIccidProperty[] = "Iccid";
const char kMccMncProperty[] = "MccMnc";
const char kNameProperty[] = "Name";
const char kNicknameProperty[] = "Nickname";
const char kProfileClassProperty[] = "ProfileClass";
const char kServiceProviderProperty[] = "ServiceProvider";
const char kStateProperty[] = "State";

}  // namespace profile

}  // namespace hermes

#endif  // SYSTEM_API_DBUS_HERMES_DBUS_CONSTANTS_H_
