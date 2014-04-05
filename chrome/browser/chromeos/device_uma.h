// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_UMA_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_UMA_H_

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/message_loop/message_loop.h"

template <typename T> struct DefaultSingletonTraits;

namespace chromeos {

// A class to record devices' input event details via the UMA system
class DeviceUMA : public base::MessageLoopForUI::Observer {
 public:
  // Getting instance starts the class automatically if it hasn't been
  // started before.
  static DeviceUMA* GetInstance();

  void Stop();

 private:
  friend struct DefaultSingletonTraits<DeviceUMA>;

  DeviceUMA();
  virtual ~DeviceUMA();

  // Start and stop observing events.
  void AddMessageLoopObserver();
  void RemoveMessageLoopObserver();

  // MessageLoopForUI::Observer overrides.
  virtual void WillProcessEvent(const base::NativeEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE;

  // Check CrOS touchpad events to see if the metrics gesture is present
  void CheckTouchpadEvent(const base::NativeEvent& event);

  // Check the incoming events for interesting patterns that we care about.
  void CheckIncomingEvent(const base::NativeEvent& event);

  bool is_observing_;

  DISALLOW_COPY_AND_ASSIGN(DeviceUMA);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_UMA_H_
