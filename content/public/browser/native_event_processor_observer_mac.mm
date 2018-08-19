// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_event_processor_observer_mac.h"

#import <AppKit/AppKit.h>

#include "base/observer_list.h"
#include "base/time/time.h"

namespace content {

namespace {

base::TimeTicks TimeTicksForEvent(NSEvent* event) {
  // NSEvent.timestamp gives the creation time of the event in seconds
  // since system startup. The baseline it should be compared agaainst is
  // NSProcessInfo.systemUptime. To convert to base::TimeTicks, we take
  // the difference and subtract from base::TimeTicks::Now().
  // Observations:
  //  1) This implementation is fast, since both systemUptime and
  //  base::TimeTicks::Now() use the commpage [no syscalls].
  //  2) systemUptime's implementation uses mach_absolute_time() -- see
  //  CoreFoundation.framework. Presumably, so does NSEvent.timestamp.
  //  mach_absolute_time() does not advance while the machine is asleep.
  NSTimeInterval current_system_uptime =
      [[NSProcessInfo processInfo] systemUptime];
  return base::TimeTicks::Now() +
         base::TimeDelta::FromSecondsD(event.timestamp - current_system_uptime);
}

}  // namespace

ScopedNotifyNativeEventProcessorObserver::
    ScopedNotifyNativeEventProcessorObserver(
        base::ObserverList<NativeEventProcessorObserver>::Unchecked*
            observer_list,
        NSEvent* event)
    : observer_list_(observer_list), event_(event) {
  for (auto& observer : *observer_list_)
    observer.WillRunNativeEvent(event_);
}

ScopedNotifyNativeEventProcessorObserver::
    ~ScopedNotifyNativeEventProcessorObserver() {
  base::TimeTicks event_creation_time;
  for (auto& obs : *observer_list_) {
    // Compute the value in the loop to avoid doing work if there are no
    // observers.
    if (event_creation_time.is_null())
      event_creation_time = TimeTicksForEvent(event_);
    obs.DidRunNativeEvent(event_, event_creation_time);
  }
}

}  // namespace content
