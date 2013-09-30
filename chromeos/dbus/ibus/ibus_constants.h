// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_
#define CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_

namespace chromeos {

namespace ibus {

const char kServiceName[] = "org.freedesktop.IBus";

// Constants used to receive NameOwner signals from ibus-daemon. Note that
// "org.freedesktop.DBus" is used instead of "org.freedesktop.IBus" for these
// signals.
const char kDBusServiceName[] = "org.freedesktop.DBus";
const char kDBusObjectPath[] = "/org/freedesktop/DBus";
const char kDBusInterface[] = "org.freedesktop.DBus";
const char kGetNameOwnerMethod[] = "GetNameOwner";
const char kNameOwnerChangedSignal[] = "NameOwnerChanged";

namespace bus {
const char kServicePath[] = "/org/freedesktop/IBus";
const char kServiceInterface[] = "org.freedesktop.IBus";
const char kCreateInputContextMethod[] = "CreateInputContext";
const char kRegisterComponentMethod[] = "RegisterComponent";
const char kSetGlobalEngineMethod[] = "SetGlobalEngine";
const char kExitMethod[] = "Exit";
}  // namespace bus

namespace input_context {
const char kServiceInterface[] = "org.freedesktop.IBus.InputContext";
const char kForwardKeyEventSignal[] = "ForwardKeyEvent";
const char kHidePreeditTextSignal[] = "HidePreeditText";
const char kShowPreeditTextSignal[] = "ShowPreeditText";
const char kUpdatePreeditTextSignal[] = "UpdatePreeditText";
const char kDeleteSurroundingTextSignal[] = "DeleteSurroundingText";
const char kFocusInMethod[] = "FocusIn";
const char kFocusOutMethod[] = "FocusOut";
const char kResetMethod[] = "Reset";
const char kSetCapabilitiesMethod[] = "SetCapabilities";
const char kSetCursorLocationMethod[] = "SetCursorLocation";
const char kProcessKeyEventMethod[] = "ProcessKeyEvent";
const char kSetSurroundingTextMethod[] = "SetSurroundingText";
const char kPropertyActivateMethod[] = "PropertyActivate";
}  // namespace input_context

namespace engine_factory {
const char kServicePath[] = "/org/freedesktop/IBus/Factory";
const char kServiceInterface[] = "org.freedesktop.IBus.Factory";
const char kCreateEngineMethod[] = "CreateEngine";
}  // namespace engine_factory

namespace config {
const char kServiceName[] = "org.freedesktop.IBus.Config";
const char kServicePath[] = "/org/freedesktop/IBus/Config";
const char kServiceInterface[] = "org.freedesktop.IBus.Config";
const char kSetValueMethod[] = "SetValue";
}  // namespace config

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_
