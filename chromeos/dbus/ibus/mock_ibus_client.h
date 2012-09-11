// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_MOCK_IBUS_CLIENT_H_
#define CHROMEOS_DBUS_IBUS_MOCK_IBUS_CLIENT_H_

#include <string>
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_client.h"

namespace chromeos {

class MockIBusClient : public IBusClient {
 public:
  MockIBusClient();
  virtual ~MockIBusClient();

  typedef base::Callback<void(const std::string& client_name,
                              const CreateInputContextCallback& callback,
                              const ErrorCallback& error_callback)>
      CreateInputContextHandler;
  typedef base::Callback<void(const ibus::IBusComponent& ibus_component,
                              const RegisterComponentCallback& callback,
                              const ErrorCallback& error_callback)>
      RegisterComponentHandler;

  // IBusClient override.
  virtual void CreateInputContext(const std::string& client_name,
                                  const CreateInputContextCallback& callback,
                                  const ErrorCallback& error_callback) OVERRIDE;

  // IBusClient override.
  virtual void RegisterComponent(const ibus::IBusComponent& ibus_component,
                                 const RegisterComponentCallback& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;

  // IBusClient override.
  virtual void SetGlobalEngine(const std::string& engine_name,
                               const ErrorCallback& error_callback) OVERRIDE;

  // IBusClient override.
  virtual void Exit(ExitOption option,
                    const ErrorCallback& error_callback) OVERRIDE;

  // Function handler for CreateInputContext. The CreateInputContext function
  // invokes |create_input_context_handler_| unless it's not null.
  void set_create_input_context_handler(
      const CreateInputContextHandler& handler) {
    create_input_context_handler_ = handler;
  }

  // Function handler for RegisterComponent. The RegisterComponent function
  // invokes |register_component_handler_| unless it's not null.
  void set_register_component_handler(
      const RegisterComponentHandler& handler) {
    register_component_handler_ = handler;
  }

  // Call count of CreateInputContext().
  int create_input_context_call_count() const {
    return create_input_context_call_count_;
  }

  // Call count of RegisterComponent().
  int register_component_call_count() const {
    return register_component_call_count_;
  }

 private:
  CreateInputContextHandler create_input_context_handler_;
  RegisterComponentHandler register_component_handler_;
  int create_input_context_call_count_;
  int register_component_call_count_;

  DISALLOW_COPY_AND_ASSIGN(MockIBusClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_MOCK_IBUS_CLIENT_H_
