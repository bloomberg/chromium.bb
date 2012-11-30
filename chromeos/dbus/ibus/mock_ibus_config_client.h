// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_CONFIG_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_CONFIG_CLIENT_H_

#include <string>
#include <vector>

#include "chromeos/dbus/ibus/ibus_config_client.h"

namespace chromeos {

class MockIBusConfigClient : public IBusConfigClient {
 public:
  MockIBusConfigClient();
  virtual ~MockIBusConfigClient();
  virtual void InitializeAsync(const OnIBusConfigReady& onready) OVERRIDE;

  virtual void SetStringValue(const std::string& key,
                              const std::string& section,
                              const std::string& value,
                              const ErrorCallback& error_callback) OVERRIDE;

  virtual void SetIntValue(const std::string& key,
                           const std::string& section,
                           int value,
                           const ErrorCallback& error_callback) OVERRIDE;

  virtual void SetBoolValue(const std::string& key,
                            const std::string& section,
                            bool value,
                            const ErrorCallback& error_callback) OVERRIDE;

  virtual void SetStringListValue(const std::string& key,
                                  const std::string& section,
                                  const std::vector<std::string>& value,
                                  const ErrorCallback& error_callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIBusConfigClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_CONFIG_CLIENT_H_
