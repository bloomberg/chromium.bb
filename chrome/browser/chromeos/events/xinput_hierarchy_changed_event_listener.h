// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_XINPUT_HIERARCHY_CHANGED_EVENT_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_XINPUT_HIERARCHY_CHANGED_EVENT_LISTENER_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/device_hierarchy_observer.h"
#include "ui/events/platform/platform_event_observer.h"

typedef union _XEvent XEvent;

namespace chromeos {

// XInputHierarchyChangedEventListener listens for an XI_HierarchyChanged event,
// which is sent to Chrome when X detects a system or USB keyboard (or mouse),
// then tells X to change the current XKB keyboard layout. Start by just calling
// instance() to get it going.
class XInputHierarchyChangedEventListener : public ui::PlatformEventObserver {
 public:
  static XInputHierarchyChangedEventListener* GetInstance();

  void Stop();

  void AddObserver(DeviceHierarchyObserver* observer);
  void RemoveObserver(DeviceHierarchyObserver* observer);

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and const/dest private as recommended for Singletons.
  friend struct DefaultSingletonTraits<XInputHierarchyChangedEventListener>;

  XInputHierarchyChangedEventListener();
  virtual ~XInputHierarchyChangedEventListener();

  // ui::PlatformEventObserver:
  virtual void WillProcessEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const ui::PlatformEvent& event) OVERRIDE;

  // Returns true if the event was processed, false otherwise.
  void ProcessedXEvent(XEvent* xevent);

  // Notify observers that a device has been added/removed.
  void NotifyDeviceHierarchyChanged();

  bool stopped_;

  ObserverList<DeviceHierarchyObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(XInputHierarchyChangedEventListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_XINPUT_HIERARCHY_CHANGED_EVENT_LISTENER_H_
