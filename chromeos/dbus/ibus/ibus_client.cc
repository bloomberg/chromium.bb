// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "chromeos/dbus/ibus/ibus_component.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// The IBusClient implementation.
class IBusClientImpl : public IBusClient {
 public:
  explicit IBusClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(ibus::kServiceName,
                                   dbus::ObjectPath(ibus::bus::kServicePath))),
        weak_ptr_factory_(this) {
  }

  virtual ~IBusClientImpl() {}

  // IBusClient override.
  virtual void CreateInputContext(
      const std::string& client_name,
      const CreateInputContextCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!callback.is_null());
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::bus::kServiceInterface,
                                 ibus::bus::kCreateInputContextMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(client_name);
    proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusClientImpl::OnCreateInputContext,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback),
        base::Bind(&IBusClientImpl::OnDBusMethodCallFail,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback));
  }

  // IBusClient override.
  virtual void RegisterComponent(
      const ibus::IBusComponent& ibus_component,
      const RegisterComponentCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!callback.is_null());
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::bus::kServiceInterface,
                                 ibus::bus::kRegisterComponentMethod);
    dbus::MessageWriter writer(&method_call);
    ibus::AppendIBusComponent(ibus_component, &writer);
    proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusClientImpl::OnRegisterComponent,
                   weak_ptr_factory_.GetWeakPtr(),
                   callback,
                   error_callback),
        base::Bind(&IBusClientImpl::OnDBusMethodCallFail,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback));
  }

  // IBusClient override.
  virtual void SetGlobalEngine(const std::string& engine_name,
                               const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::bus::kServiceInterface,
                                 ibus::bus::kSetGlobalEngineMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(engine_name);
    proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusClientImpl::OnSetGlobalEngine,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback),
        base::Bind(&IBusClientImpl::OnDBusMethodCallFail,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback));
  }

  // IBusClient override.
  virtual void Exit(ExitOption option,
                    const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::bus::kServiceInterface,
                                 ibus::bus::kExitMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(option == RESTART_IBUS_DAEMON);
    proxy_->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusClientImpl::OnExit,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback),
        base::Bind(&IBusClientImpl::OnDBusMethodCallFail,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback));
  }

 private:
  // Handles responses of CreateInputContext method calls.
  void OnCreateInputContext(const CreateInputContextCallback& callback,
                            const ErrorCallback& error_callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Cannot get input context: response is NULL.";
      error_callback.Run();
      return;
    }
    dbus::MessageReader reader(response);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      // The IBus message structure may be changed.
      LOG(ERROR) << "Invalid response: " << response->ToString();
      error_callback.Run();
      return;
    }
    callback.Run(object_path);
  }

  // Handles responses of RegisterComponent method calls.
  void OnRegisterComponent(const RegisterComponentCallback& callback,
                           const ErrorCallback& error_callback,
                           dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Response is NULL.";
      error_callback.Run();
      return;
    }
    callback.Run();
  }

  // Handles responses of RegisterComponent method calls.
  void OnSetGlobalEngine(const ErrorCallback& error_callback,
                         dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Response is NULL.";
      error_callback.Run();
      return;
    }
  }

  // Handles responses of RegisterComponent method calls.
  void OnExit(const ErrorCallback& error_callback,
              dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Response is NULL.";
      error_callback.Run();
      return;
    }
  }

  // Handles error response of RegisterComponent method call.
  void OnDBusMethodCallFail(const ErrorCallback& error_callback,
                            dbus::ErrorResponse* response) {
    error_callback.Run();
  }

  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<IBusClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusClientImpl);
};

// A stub implementation of IBusClient.
class IBusClientStubImpl : public IBusClient {
 public:
  IBusClientStubImpl() {}
  virtual ~IBusClientStubImpl() {}

  virtual void CreateInputContext(
      const std::string& client_name,
      const CreateInputContextCallback & callback,
      const ErrorCallback& error_callback) OVERRIDE {}

  virtual void RegisterComponent(
      const ibus::IBusComponent& ibus_component,
      const RegisterComponentCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE {}

  virtual void SetGlobalEngine(const std::string& engine_name,
                               const ErrorCallback& error_callback) OVERRIDE {}

  virtual void Exit(ExitOption option,
                    const ErrorCallback& error_callback) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusClientStubImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusClient

IBusClient::IBusClient() {}

IBusClient::~IBusClient() {}

// static
IBusClient* IBusClient::Create(DBusClientImplementationType type,
                               dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new IBusClientImpl(bus);
  }
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new IBusClientStubImpl();
}

}  // namespace chromeos
