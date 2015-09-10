// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_UMA_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_UMA_H_

#include "base/basictypes.h"
#include "ui/events/platform/platform_event_observer.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace chromeos {

// A class to record devices' input event details via the UMA system
class DeviceUMA : public ui::PlatformEventObserver {
 public:
  // Getting instance starts the class automatically if it hasn't been
  // started before.
  static DeviceUMA* GetInstance();

  void Stop();

 private:
  friend struct base::DefaultSingletonTraits<DeviceUMA>;

  DeviceUMA();
  ~DeviceUMA() override;

  // ui::PlatformEventObserver:
  void WillProcessEvent(const ui::PlatformEvent& event) override;
  void DidProcessEvent(const ui::PlatformEvent& event) override;

  // Check CrOS touchpad events to see if the metrics gesture is present
  void CheckTouchpadEvent(XEvent* event);

  // Check the incoming events for interesting patterns that we care about.
  void CheckIncomingEvent(XEvent* event);

  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(DeviceUMA);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_UMA_H_
