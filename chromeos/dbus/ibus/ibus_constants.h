// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_
#define CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_

namespace chromeos {

namespace ibus {

const char kServiceName[] = "org.freedesktop.IBus";

namespace bus {
const char kServicePath[] = "/org/freedesktop/IBus";
const char kServiceInterface[] = "org.freedesktop.IBus";
const char kCreateInputContextMethod[] = "CreateInputContext";
const char kRegisterComponentMethod[] = "RegisterComponent";
}  // namespace bus

namespace input_context {
const char kServiceInterface[] = "org.freedesktop.IBus.InputContext";
const char kCommitTextSignal[] = "CommitText";
const char kForwardKeyEventSignal[] = "ForwardKeyEvent";
const char kHidePreeditTextSignal[] = "HidePreeditText";
const char kShowPreeditTextSignal[] = "ShowPreeditText";
const char kUpdatePreeditTextSignal[] = "UpdatePreeditText";
const char kFocusInMethod[] = "FocusIn";
const char kFocusOutMethod[] = "FocusOut";
const char kResetMethod[] = "Reset";
const char kSetCapabilitiesMethod[] = "SetCapabilities";
const char kSetCursorLocationMethod[] = "SetCursorLocation";
const char kProcessKeyEventMethod[] = "ProcessKeyEvent";
}  // namespace input_context

namespace engine_factory {
const char kServicePath[] = "/org/freedesktop/IBus/Factory";
const char kServiceInterface[] = "org.freedesktop.IBus.Factory";
const char kCreateEngineMethod[] = "CreateEngine";
}  // namespace engine factory

namespace engine {
const char kServicePathPrefix[] = "/org/freedesktop/IBus/Engine/";
}  // namespace engine

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_
