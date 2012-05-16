// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_GSM_SMS_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_GSM_SMS_CLIENT_H_
#pragma once

#include "base/values.h"
#include "chromeos/dbus/gsm_sms_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockGsmSMSClient : public GsmSMSClient {
 public:
  MockGsmSMSClient();
  virtual ~MockGsmSMSClient();

  MOCK_METHOD3(SetSmsReceivedHandler, void(const std::string& service_name,
                                           const dbus::ObjectPath& object_path,
                                           const SmsReceivedHandler& handler));
  MOCK_METHOD2(ResetSmsReceivedHandler,
               void(const std::string& service_name,
                    const dbus::ObjectPath& object_path));
  MOCK_METHOD4(Delete, void(const std::string& service_name,
                            const dbus::ObjectPath& object_path,
                            uint32 index,
                            const DeleteCallback& callback));
  MOCK_METHOD4(Get, void(const std::string& service_name,
                         const dbus::ObjectPath& object_path,
                         uint32 index,
                         const GetCallback& callback));
  MOCK_METHOD3(List, void(const std::string& service_name,
                          const dbus::ObjectPath& object_path,
                          const ListCallback& callback));
  MOCK_METHOD2(RequestUpdate, void(const std::string& service_name,
                                   const dbus::ObjectPath& object_path));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_GSM_SMS_CLIENT_H_
