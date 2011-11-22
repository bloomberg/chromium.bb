// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/xinput_hierarchy_changed_event_listener.h"

namespace chromeos {

void XInputHierarchyChangedEventListener::Init() {
  MessageLoopForUI::current()->AddObserver(this);
}

void XInputHierarchyChangedEventListener::StopImpl() {
  MessageLoopForUI::current()->RemoveObserver(this);
}

base::EventStatus XInputHierarchyChangedEventListener::WillProcessEvent(
    const base::NativeEvent& event) {
  // There may be multiple listeners for the XI_HierarchyChanged event. So
  // always return EVENT_CONTINUE to make sure all the listeners receive the
  // event.
  ProcessedXEvent(event);
  return base::EVENT_CONTINUE;
}

void XInputHierarchyChangedEventListener::DidProcessEvent(
    const base::NativeEvent& event) {
}

}  // namespace chromeos
