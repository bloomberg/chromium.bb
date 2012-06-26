// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_CLIENT_H_
#pragma once

#include <string>
#include "chromeos/dbus/ibus/ibus_client.h"

namespace chromeos {

class MockIBusClient : public IBusClient {
 public:
  MockIBusClient();
  virtual ~MockIBusClient();

  virtual void CreateInputContext(const std::string& client_name,
                                  const CreateInputContextCallback& callback,
                                  const ErrorCallback& error_callback) OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_CLIENT_H_
