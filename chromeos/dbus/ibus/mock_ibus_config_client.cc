// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_config_client.h"

#include <vector>

namespace chromeos {

MockIBusConfigClient::MockIBusConfigClient() {
}

MockIBusConfigClient::~MockIBusConfigClient() {}

void MockIBusConfigClient::InitializeAsync(const OnIBusConfigReady& onready) {
}

void MockIBusConfigClient::SetStringValue(const std::string& key,
                                          const std::string& section,
                                          const std::string& value,
                                          const ErrorCallback& error_callback) {
}

void MockIBusConfigClient::SetIntValue(const std::string& key,
                                       const std::string& section,
                                       int value,
                                       const ErrorCallback& error_callback) {
}

void MockIBusConfigClient::SetBoolValue(const std::string& key,
                                        const std::string& section,
                                        bool value,
                                        const ErrorCallback& error_callback) {
}

void MockIBusConfigClient::SetStringListValue(
    const std::string& key,
    const std::string& section,
    const std::vector<std::string>& value,
    const ErrorCallback& error_callback) {
}

}  // namespace chromeos
