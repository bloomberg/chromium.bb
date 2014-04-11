// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/xinput_hierarchy_changed_event_listener.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"

namespace chromeos {
namespace {

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
        chromeos::input_method::InputMethodManager::Get();
    chromeos::input_method::ImeKeyboard* keyboard =
        input_method_manager->GetImeKeyboard();
    keyboard->ReapplyCurrentModifierLockStatus();
    keyboard->ReapplyCurrentKeyboardLayout();
  }
}

}  // namespace

// static
XInputHierarchyChangedEventListener*
XInputHierarchyChangedEventListener::GetInstance() {
  return Singleton<XInputHierarchyChangedEventListener>::get();
}

XInputHierarchyChangedEventListener::XInputHierarchyChangedEventListener()
    : stopped_(false) {
  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

XInputHierarchyChangedEventListener::~XInputHierarchyChangedEventListener() {
  Stop();
}

void XInputHierarchyChangedEventListener::Stop() {
  if (stopped_)
    return;

  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  stopped_ = true;
}

void XInputHierarchyChangedEventListener::AddObserver(
    DeviceHierarchyObserver* observer) {
  observer_list_.AddObserver(observer);
}

void XInputHierarchyChangedEventListener::RemoveObserver(
    DeviceHierarchyObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void XInputHierarchyChangedEventListener::WillProcessEvent(
    const ui::PlatformEvent& event) {
  ProcessedXEvent(event);
}

void XInputHierarchyChangedEventListener::DidProcessEvent(
    const ui::PlatformEvent& event) {
}

void XInputHierarchyChangedEventListener::ProcessedXEvent(XEvent* xevent) {
  if (xevent->xcookie.type != GenericEvent)
    return;

  XGenericEventCookie* cookie = &(xevent->xcookie);

  if (cookie->evtype == XI_HierarchyChanged) {
    XIHierarchyEvent* event = static_cast<XIHierarchyEvent*>(cookie->data);
    HandleHierarchyChangedEvent(event, &observer_list_);
    if (event->flags & XIDeviceEnabled || event->flags & XIDeviceDisabled)
      NotifyDeviceHierarchyChanged();
  }
}

void XInputHierarchyChangedEventListener::NotifyDeviceHierarchyChanged() {
  FOR_EACH_OBSERVER(DeviceHierarchyObserver,
                    observer_list_,
                    DeviceHierarchyChanged());
}

}  // namespace chromeos
