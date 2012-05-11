// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
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
      : proxy_(bus->GetObjectProxy(kIBusServiceName,
                                   dbus::ObjectPath(kIBusServicePath))),
        weak_ptr_factory_(this) {
  }

  virtual ~IBusClientImpl() {}

  // IBusClient override.
  virtual void CreateInputContext(
      const std::string& client_name,
      const CreateInputContextCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(kIBusServiceInterface,
                                 kIBusBusCreateInputContextMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(client_name);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&IBusClientImpl::OnCreateInputContext,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  callback));
  }

 private:
  // Handles responses of CreateInputContext method calls.
  void OnCreateInputContext(const CreateInputContextCallback& callback,
                            dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "Cannot get input context: " << response->ToString();
      return;
    }
    dbus::MessageReader reader(response);
    dbus::ObjectPath object_path;
    if (!reader.PopObjectPath(&object_path)) {
      // The IBus message structure may be changed.
      LOG(ERROR) << "Invalid response: " << response->ToString();
      return;
    }
    callback.Run(object_path);
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
      const CreateInputContextCallback & callback) OVERRIDE {}

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
