// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chromeos/ime/xkeyboard.h"
#include "ui/base/x/x11_util.h"

namespace chromeos {
namespace {

// Gets the major opcode for XInput2. Returns -1 on error.
int GetXInputOpCode() {
  static const char kExtensionName[] = "XInputExtension";
  int xi_opcode = -1;
  int event;
  int error;

  if (!XQueryExtension(
          ui::GetXDisplay(), kExtensionName, &xi_opcode, &event, &error)) {
    VLOG(1) << "X Input extension not available: error=" << error;
    return -1;
  }
  return xi_opcode;
}

// Checks the |event| and asynchronously sets the XKB layout when necessary.
void HandleHierarchyChangedEvent(
    XIHierarchyEvent* event,
    ObserverList<DeviceHierarchyObserver>* observer_list) {
  if (!(event->flags & (XISlaveAdded | XISlaveRemoved)))
    return;

  bool update_keyboard_status = false;
  for (int i = 0; i < event->num_info; ++i) {
    XIHierarchyInfo* info = &event->info[i];
    if ((info->flags & XISlaveAdded) && (info->use == XIFloatingSlave)) {
      FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                        *observer_list,
                        DeviceAdded(info->deviceid));
      update_keyboard_status = true;
    } else if (info->flags & XISlaveRemoved) {
      // Can't check info->use here; it appears to always be 0.
      FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                        *observer_list,
                        DeviceRemoved(info->deviceid));
    }
  }

  if (update_keyboard_status) {
    chromeos::input_method::InputMethodManager* input_method_manager =
        chromeos::input_method::GetInputMethodManager();
    chromeos::input_method::XKeyboard* xkeyboard =
        input_method_manager->GetXKeyboard();
    xkeyboard->ReapplyCurrentModifierLockStatus();
    xkeyboard->ReapplyCurrentKeyboardLayout();
  }
}

}  // namespace

// static
XInputHierarchyChangedEventListener*
XInputHierarchyChangedEventListener::GetInstance() {
  return Singleton<XInputHierarchyChangedEventListener>::get();
}

XInputHierarchyChangedEventListener::XInputHierarchyChangedEventListener()
    : stopped_(false),
      xiopcode_(GetXInputOpCode()) {
  Init();
}

XInputHierarchyChangedEventListener::~XInputHierarchyChangedEventListener() {
  Stop();
}

void XInputHierarchyChangedEventListener::Stop() {
  if (stopped_)
    return;

  StopImpl();
  stopped_ = true;
  xiopcode_ = -1;
}

void XInputHierarchyChangedEventListener::AddObserver(
    DeviceHierarchyObserver* observer) {
  observer_list_.AddObserver(observer);
}

void XInputHierarchyChangedEventListener::RemoveObserver(
    DeviceHierarchyObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool XInputHierarchyChangedEventListener::ProcessedXEvent(XEvent* xevent) {
  if ((xevent->xcookie.type != GenericEvent) ||
      (xevent->xcookie.extension != xiopcode_)) {
    return false;
  }

  XGenericEventCookie* cookie = &(xevent->xcookie);
  bool handled = false;

  if (cookie->evtype == XI_HierarchyChanged) {
    XIHierarchyEvent* event = static_cast<XIHierarchyEvent*>(cookie->data);
    HandleHierarchyChangedEvent(event, &observer_list_);
    if (event->flags & XIDeviceEnabled || event->flags & XIDeviceDisabled)
      NotifyDeviceHierarchyChanged();
    handled = true;
  } else if (cookie->evtype == XI_KeyPress || cookie->evtype == XI_KeyRelease) {
    XIDeviceEvent* xiev = reinterpret_cast<XIDeviceEvent*>(cookie->data);
    if (xiev->deviceid == xiev->sourceid) {
      FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                        observer_list_,
                        DeviceKeyPressedOrReleased(xiev->deviceid));
      handled = true;
    }
  }

  return handled;
}

void XInputHierarchyChangedEventListener::NotifyDeviceHierarchyChanged() {
  FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                    observer_list_,
                    DeviceHierarchyChanged());
}

}  // namespace chromeos
