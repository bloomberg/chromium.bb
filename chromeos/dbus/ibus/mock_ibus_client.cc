// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/mock_ibus_client.h"

namespace chromeos {

MockIBusClient::MockIBusClient()
    : create_input_context_call_count_(0) {
}

MockIBusClient::~MockIBusClient() {}

void MockIBusClient::CreateInputContext(
    const std::string& client_name,
    const CreateInputContextCallback& callback,
    const ErrorCallback& error_callback) {
  create_input_context_call_count_ ++;
  if (!create_input_context_handler_.is_null())
    create_input_context_handler_.Run(client_name, callback, error_callback);
}

void MockIBusClient::RegisterComponent(
    const ibus::IBusComponent& ibus_component,
    const RegisterComponentCallback& callback,
    const ErrorCallback& error_callback) {
  register_component_call_count_ ++;
  if (!register_component_handler_.is_null())
    register_component_handler_.Run(ibus_component, callback, error_callback);
}

void MockIBusClient::SetGlobalEngine(const std::string& engine_name,
                                     const ErrorCallback& error_callback) {
}

void MockIBusClient::Exit(ExitOption option,
                          const ErrorCallback& error_callback) {
}

}  // namespace chromeos
