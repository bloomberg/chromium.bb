// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_CASHEW_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_CASHEW_CLIENT_H_
#pragma once

#include "chromeos/dbus/cashew_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCashewClient : public CashewClient {
 public:
  MockCashewClient();
  virtual ~MockCashewClient();

  MOCK_METHOD1(SetDataPlansUpdateHandler,
               void(const DataPlansUpdateHandler& handler));
  MOCK_METHOD0(ResetDataPlansUpdateHandler, void());
  MOCK_METHOD1(RequestDataPlansUpdate, void(const std::string& service));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_CASHEW_CLIENT_H_
