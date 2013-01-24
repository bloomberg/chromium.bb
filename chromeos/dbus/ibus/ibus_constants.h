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
const char kSetSurroundingTextMethod[] = "SetSurroundingText";
const char kPropertyActivateMethod[] = "PropertyActivate";
}  // namespace input_context

namespace engine_factory {
const char kServicePath[] = "/org/freedesktop/IBus/Factory";
const char kServiceInterface[] = "org.freedesktop.IBus.Factory";
const char kCreateEngineMethod[] = "CreateEngine";
}  // namespace engine factory

namespace engine {
const char kServicePathPrefix[] = "/org/freedesktop/IBus/Engine/";
const char kServiceInterface[] = "org.freedesktop.IBus.Engine";
const char kFocusInMethod[] = "FocusIn";
const char kFocusOutMethod[] = "FocusOut";
const char kEnableMethod[] = "Enable";
const char kDisableMethod[] = "Disable";
const char kPropertyActivateMethod[] = "PropertyActivate";
const char kPropertyShowMethod[] = "PropertyShow";
const char kPropertyHideMethod[] = "PropertyHide";
const char kSetCapabilityMethod[] = "SetCapability";
const char kResetMethod[] = "Reset";
const char kProcessKeyEventMethod[] = "ProcessKeyEvent";
const char kCandidateClickedMethod[] = "CandidateClicked";
const char kSetSurroundingTextMethod[] = "SetSurroundingText";
const char kRegisterPropertiesSignal[] = "RegisterProperties";
const char kUpdatePreeditSignal[] = "UpdatePreeditText";
const char kUpdateAuxiliaryTextSignal[] = "UpdateAuxiliaryText";
const char kUpdateLookupTableSignal[] = "UpdateLookupTable";
const char kUpdatePropertySignal[] = "UpdateProperty";
const char kForwardKeyEventSignal[] = "ForwardKeyEvent";
const char kRequireSurroundingTextSignal[] = "RequireSurroundingText";
const char kCommitTextSignal[] = "CommitText";
}  // namespace engine

namespace panel {
const char kServiceName[] = "org.freedesktop.IBus.Panel";
const char kServicePath[] = "/org/freedesktop/IBus/Panel";
const char kServiceInterface[] = "org.freedesktop.IBus.Panel";
const char kUpdateLookupTableMethod[] = "UpdateLookupTable";
const char kHideLookupTableMethod[] = "HideLookupTable";
const char kUpdateAuxiliaryTextMethod[] = "UpdateAuxiliaryText";
const char kHideAuxiliaryTextMethod[] = "HideAuxiliaryText";
const char kUpdatePreeditTextMethod[] = "UpdatePreeditText";
const char kHidePreeditTextMethod[] = "HidePreeditText";
const char kRegisterPropertiesMethod[] = "RegisterProperties";
const char kUpdatePropertyMethod[] = "UpdateProperty";
const char kCandidateClickedSignal[] = "CandidateClicked";
const char kCursorUpSignal[] = "CursorUp";
const char kCursorDownSignal[] = "CursorDown";
const char kPageUpSignal[] = "PageUp";
const char kPageDownSignal[] = "PageDown";

// Methods to be just ignored. We do not use these methods in the UI.
// See http://crbug.com/164525.
const char kFocusInMethod[] = "FocusIn";
const char kFocusOutMethod[] = "FocusOut";
const char kStateChangedMethod[] = "StateChanged";
}  // namespace panel

// Following variables indicate state of IBusProperty.
enum IBusPropertyState {
  IBUS_PROPERTY_STATE_UNCHECKED = 0,
  IBUS_PROPERTY_STATE_CHECKED = 1,
  IBUS_PROPERTY_STATE_INCONSISTENT = 2,
};

// Following button indicator value is introduced from
// http://developer.gnome.org/gdk/stable/gdk-Event-Structures.html#GdkEventButton
enum IBusMouseButton {
  IBUS_MOUSE_BUTTON_LEFT = 1U,
  IBUS_MOUSE_BUTTON_MIDDLE = 2U,
  IBUS_MOUSE_BUTTON_RIGHT = 3U,
};

namespace config {
const char kServiceName[] = "org.freedesktop.IBus.Config";
const char kServicePath[] = "/org/freedesktop/IBus/Config";
const char kServiceInterface[] = "org.freedesktop.IBus.Config";
const char kSetValueMethod[] = "SetValue";
}  // namespace config

// We can't use ui/gfx/rect.h in chromeos/, so we should use ibus::Rect instead.
struct Rect {
 Rect() : x(0), y(0), width(0), height(0) {}
 Rect(int x, int y, int width, int height)
     : x(x),
       y(y),
       width(width),
       height(height) {}
 int x;
 int y;
 int width;
 int height;
};

}  // namespace ibus
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_CONSTANTS_H_
