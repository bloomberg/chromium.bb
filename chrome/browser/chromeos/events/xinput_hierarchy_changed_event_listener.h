// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_XINPUT_HIERARCHY_CHANGED_EVENT_LISTENER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_XINPUT_HIERARCHY_CHANGED_EVENT_LISTENER_H_

#include "base/memory/singleton.h"
#include "base/observer_list.h"
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

 private:
  // Defines the delete on exit Singleton traits we like.  Best to have this
  // and const/dest private as recommended for Singletons.
  friend struct base::DefaultSingletonTraits<
      XInputHierarchyChangedEventListener>;

  XInputHierarchyChangedEventListener();
  ~XInputHierarchyChangedEventListener() override;

  // ui::PlatformEventObserver:
  void WillProcessEvent(const ui::PlatformEvent& event) override;
  void DidProcessEvent(const ui::PlatformEvent& event) override;

  // Returns true if the event was processed, false otherwise.
  void ProcessedXEvent(XEvent* xevent);

  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(XInputHierarchyChangedEventListener);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_XINPUT_HIERARCHY_CHANGED_EVENT_LISTENER_H_
