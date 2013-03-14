// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SYSTEM_CLOCK_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SYSTEM_CLOCK_CLIENT_H_

#include "chromeos/dbus/system_clock_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

// SystemClockClient is used to communicate with the system clock.
class MockSystemClockClient : public SystemClockClient {
 public:
  MockSystemClockClient();
  virtual ~MockSystemClockClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(HasObserver, bool(Observer*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSystemClockClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SYSTEM_CLOCK_CLIENT_H_
