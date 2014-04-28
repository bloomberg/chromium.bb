// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SYSTEM_CLOCK_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SYSTEM_CLOCK_CLIENT_H_

#include "chromeos/dbus/system_clock_client.h"

namespace chromeos {

// A fake implementation of SystemClockClient. This class does nothing.
class CHROMEOS_EXPORT FakeSystemClockClient : public SystemClockClient {
 public:
  FakeSystemClockClient();
  virtual ~FakeSystemClockClient();

  // SystemClockClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void SetTime(int64 time_in_seconds) OVERRIDE;
  virtual bool CanSetTime() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSystemClockClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SYSTEM_CLOCK_CLIENT_H_
