// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_SENSORS_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_SENSORS_CLIENT_H_

#include <string>

#include "chrome/browser/chromeos/dbus/sensors_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSensorsClient : public SensorsClient {
 public:
  MockSensorsClient();
  virtual ~MockSensorsClient();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_SENSORS_CLIENT_H_
