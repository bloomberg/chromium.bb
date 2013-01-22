// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_config_client.h"

#include <vector>

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

// Called when the |signal| is connected.
void OnSignalConnected(const std::string& interface,
                       const std::string& signal,
                       bool succeeded) {
  LOG_IF(ERROR, !succeeded) << "Connect to " << interface << " "
                            << signal << " failed.";
}

// Called when the GetNameOwner method call is failed.
void OnGetNameOwnerFail(dbus::ErrorResponse* response) {
  // Do nothing, because method call sometimes fails due to bootstrap timing of
  // ibus-memconf.
}

// The IBusConfigClient implementation.
class IBusConfigClientImpl : public IBusConfigClient {
 public:
  explicit IBusConfigClientImpl(dbus::Bus* bus)
      : proxy_(NULL),
        bus_(bus),
        weak_ptr_factory_(this) {
  }

  virtual ~IBusConfigClientImpl() {}

  // IBusConfigClient override.
  virtual void InitializeAsync(const OnIBusConfigReady& on_ready) OVERRIDE {
    // We should check that the ibus-config daemon actually works first, so we
    // can't initialize synchronously.
    // NameOwnerChanged signal will be emitted by ibus-daemon, but from the
    // service name kDBusServiceName instead of kServiceName. The signal will be
    // used to detect start of ibus-daemon.
    dbus::ObjectProxy* dbus_proxy = bus_->GetObjectProxy(
        ibus::kDBusServiceName,
        dbus::ObjectPath(ibus::kDBusObjectPath));

    // Watch NameOwnerChanged signal which is fired when the ibus-config daemon
    // request its name ownership.
    dbus_proxy->ConnectToSignal(
        ibus::kDBusInterface,
        ibus::kNameOwnerChangedSignal,
        base::Bind(&IBusConfigClientImpl::OnNameOwnerChanged,
                   weak_ptr_factory_.GetWeakPtr(),
                   on_ready),
        base::Bind(&OnSignalConnected));

    dbus::MethodCall method_call(ibus::kDBusInterface,
                                 ibus::kGetNameOwnerMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ibus::config::kServiceName);
    dbus_proxy->CallMethodWithErrorCallback(
        &method_call,
        dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::Bind(&IBusConfigClientImpl::OnGetNameOwner,
                   weak_ptr_factory_.GetWeakPtr(),
                   on_ready),
        base::Bind(&OnGetNameOwnerFail));
  }

  // IBusConfigClient override.
  virtual void SetStringValue(const std::string& section,
                              const std::string& key,
                              const std::string& value,
                              const ErrorCallback& error_callback) OVERRIDE {
    if (!proxy_)
      return;
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
    if (!proxy_)
      return;
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
    if (!proxy_)
      return;
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
    if (!proxy_)
      return;
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
    if (!proxy_)
      return;
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

  void OnNameOwnerChanged(const OnIBusConfigReady& on_ready,
                          dbus::Signal* signal) {
    DCHECK(signal);
    std::string name;
    std::string old_owner;
    std::string new_owner;

    dbus::MessageReader reader(signal);
    if (!reader.PopString(&name) ||
        !reader.PopString(&old_owner) ||
        !reader.PopString(&new_owner)) {
      DLOG(ERROR) << "Invalid response of NameOwnerChanged."
                  << signal->ToString();
      return;
    }

    if (name != ibus::config::kServiceName)
      return;  // Not a signal for ibus-config.

    if (!old_owner.empty() || new_owner.empty()) {
      DVLOG(1) << "Unexpected name owner change: name=" << name
               << ", old_owner=" << old_owner << ", new_owner=" << new_owner;
      proxy_ = NULL;
      return;
    }

    if (proxy_)
      return;  // Already initialized.

    proxy_ = bus_->GetObjectProxy(ibus::config::kServiceName,
                                  dbus::ObjectPath(
                                      ibus::config::kServicePath));
    if (!on_ready.is_null())
      on_ready.Run();
  }

  // Handles response of GetNameOwner.
  void OnGetNameOwner(const OnIBusConfigReady& on_ready,
                      dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Response is NULL.";
      return;
    }
    std::string owner;
    dbus::MessageReader reader(response);

    if (!reader.PopString(&owner)) {
      LOG(ERROR) << "Invalid response of GetNameOwner."
                 << response->ToString();
      return;
    }

    // If the owner is empty, ibus-config daemon is not ready. So will
    // initialize object proxy on NameOwnerChanged signal.
    if (owner.empty())
      return;

    proxy_ = bus_->GetObjectProxy(ibus::config::kServiceName,
                                  dbus::ObjectPath(
                                      ibus::config::kServicePath));
    if (!on_ready.is_null())
      on_ready.Run();
  }

  dbus::ObjectProxy* proxy_;
  dbus::Bus* bus_;
  base::WeakPtrFactory<IBusConfigClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IBusConfigClientImpl);
};

// A stub implementation of IBusConfigClient.
// A configuration framework based on ibus will not be used after Extension IME
// migration.
class IBusConfigClientStubImpl : public IBusConfigClient {
 public:
  IBusConfigClientStubImpl() {}
  virtual ~IBusConfigClientStubImpl() {}
  virtual void InitializeAsync(const OnIBusConfigReady& on_ready) OVERRIDE {}
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
