// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_config_client.h"

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

// The IBusConfigClient implementation.
class IBusConfigClientImpl : public IBusConfigClient {
 public:
  explicit IBusConfigClientImpl(dbus::Bus* bus)
      : proxy_(bus->GetObjectProxy(ibus::kServiceName,
                                   dbus::ObjectPath(
                                       ibus::config::kServicePath))),
        weak_ptr_factory_(this) {
  }

  virtual ~IBusConfigClientImpl() {}

  // IBusConfigClient override.
  virtual void SetStringValue(const std::string& section,
                              const std::string& key,
                              const std::string& value,
                              const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::config::kServiceInterface,
                                 ibus::config::kSetValueMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(section);
    writer.AppendString(key);
    dbus::MessageWriter variant_writer(NULL);
    writer.OpenVariant("s", &variant_writer);
    variant_writer.AppendString(value);
    writer.CloseContainer(&variant_writer);
    CallWithDefaultCallback(&method_call, error_callback);
  }

  // IBusConfigClient override.
  virtual void SetIntValue(const std::string& section,
                           const std::string& key,
                           int value,
                           const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::config::kServiceInterface,
                                 ibus::config::kSetValueMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(section);
    writer.AppendString(key);
    dbus::MessageWriter variant_writer(NULL);
    writer.OpenVariant("i", &variant_writer);
    variant_writer.AppendInt32(value);
    writer.CloseContainer(&variant_writer);
    CallWithDefaultCallback(&method_call, error_callback);
  }

  // IBusConfigClient override.
  virtual void SetBoolValue(const std::string& section,
                            const std::string& key,
                            bool value,
                            const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::config::kServiceInterface,
                                 ibus::config::kSetValueMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(section);
    writer.AppendString(key);
    dbus::MessageWriter variant_writer(NULL);
    writer.OpenVariant("b", &variant_writer);
    variant_writer.AppendBool(value);
    writer.CloseContainer(&variant_writer);
    CallWithDefaultCallback(&method_call, error_callback);
  }

  // IBusConfigClient override.
  virtual void SetStringListValue(
      const std::string& section,
      const std::string& key,
      const std::vector<std::string>& value,
      const ErrorCallback& error_callback) OVERRIDE {
    DCHECK(!error_callback.is_null());
    dbus::MethodCall method_call(ibus::config::kServiceInterface,
                                 ibus::config::kSetValueMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(section);
    writer.AppendString(key);
    dbus::MessageWriter variant_writer(NULL);
    dbus::MessageWriter array_writer(NULL);

    writer.OpenVariant("as", &variant_writer);
    variant_writer.OpenArray("s", &array_writer);
    for (size_t i = 0; i < value.size(); ++i) {
      array_writer.AppendString(value[i]);
    }
    variant_writer.CloseContainer(&array_writer);
    writer.CloseContainer(&variant_writer);
    CallWithDefaultCallback(&method_call, error_callback);
  }

 private:
  void CallWithDefaultCallback(dbus::MethodCall* method_call,
                               const ErrorCallback& error_callback) {
    proxy_->CallMethodWithErrorCallback(
        method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusConfigClientImpl::OnSetValue,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback),
        base::Bind(&IBusConfigClientImpl::OnSetValueFail,
                   weak_ptr_factory_.GetWeakPtr(),
                   error_callback));
  }

  void OnSetValue(const ErrorCallback& error_callback,
                  dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Response is NULL.";
      error_callback.Run();
      return;
    }
  }

  void OnSetValueFail(const ErrorCallback& error_callback,
                       dbus::ErrorResponse* response) {
    error_callback.Run();
  }

  dbus::ObjectProxy* proxy_;
  base::WeakPtrFactory<IBusConfigClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusConfigClientImpl);
};

// A stub implementation of IBusConfigClient.
class IBusConfigClientStubImpl : public IBusConfigClient {
 public:
  IBusConfigClientStubImpl() {}
  virtual ~IBusConfigClientStubImpl() {}
  virtual void SetStringValue(const std::string& section,
                              const std::string& key,
                              const std::string& value,
                              const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetIntValue(const std::string& section,
                           const std::string& key,
                           int value,
                           const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetBoolValue(const std::string& section,
                            const std::string& key,
                            bool value,
                            const ErrorCallback& error_callback) OVERRIDE {}
  virtual void SetStringListValue(
      const std::string& section,
      const std::string& key,
      const std::vector<std::string>& value,
      const ErrorCallback& error_callback) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusConfigClientStubImpl);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusConfigClient

IBusConfigClient::IBusConfigClient() {}

IBusConfigClient::~IBusConfigClient() {}

// static
IBusConfigClient* IBusConfigClient::Create(DBusClientImplementationType type,
                                           dbus::Bus* bus) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION) {
    return new IBusConfigClientImpl(bus);
  }
  DCHECK_EQ(STUB_DBUS_CLIENT_IMPLEMENTATION, type);
  return new IBusConfigClientStubImpl();
}

}  // namespace chromeos
