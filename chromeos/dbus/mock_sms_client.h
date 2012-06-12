// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SMS_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SMS_CLIENT_H_
#pragma once

#include <string>

#include "base/values.h"
#include "chromeos/dbus/sms_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSMSClient : public SMSClient {
 public:
  MockSMSClient();
  virtual ~MockSMSClient();
  MOCK_METHOD3(GetAll, void(const std::string& service_name,
                            const dbus::ObjectPath& object_path,
                            const GetAllCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SMS_CLIENT_H_
