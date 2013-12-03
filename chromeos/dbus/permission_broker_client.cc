// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/permission_broker_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using permission_broker::kPermissionBrokerInterface;
using permission_broker::kPermissionBrokerServiceName;
using permission_broker::kPermissionBrokerServicePath;
using permission_broker::kRequestPathAccess;
using permission_broker::kRequestUsbAccess;

namespace chromeos {

class PermissionBrokerClientImpl : public PermissionBrokerClient {
 public:
  PermissionBrokerClientImpl() : proxy_(NULL), weak_ptr_factory_(this) {}

  virtual void RequestPathAccess(const std::string& path,
                                 const int interface_id,
                                 const ResultCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestPathAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    writer.AppendInt32(interface_id);
    proxy_->CallMethod(&method_call,
                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

  virtual void RequestUsbAccess(const uint16_t vendor_id,
                                const uint16_t product_id,
                                const int interface_id,
                                const ResultCallback& callback) OVERRIDE {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kRequestUsbAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(vendor_id);
    writer.AppendUint16(product_id);
    writer.AppendInt32(interface_id);
    proxy_->CallMethod(&method_call,
                       dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&PermissionBrokerClientImpl::OnResponse,
                                  weak_ptr_factory_.GetWeakPtr(), callback));
  }

 protected:
  virtual void Init(dbus::Bus* bus) OVERRIDE {
    proxy_ =
        bus->GetObjectProxy(kPermissionBrokerServiceName,
                            dbus::ObjectPath(kPermissionBrokerServicePath));
  }

 private:
  // Handle a DBus response from the permission broker, invoking the callback
  // that the method was originally called with with the success response.
  void OnResponse(const ResultCallback& callback, dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Access request method call failed.";
      callback.Run(false);
      return;
    }

    bool result = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result))
      LOG(WARNING) << "Could not parse response: " << response->ToString();
    callback.Run(result);
  }

  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so that it will be destroyed
  // first, invalidating its weak pointers, before the other members are
  // destroyed.
  base::WeakPtrFactory<PermissionBrokerClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBrokerClientImpl);
};

PermissionBrokerClient::PermissionBrokerClient() {}

PermissionBrokerClient::~PermissionBrokerClient() {}

PermissionBrokerClient* PermissionBrokerClient::Create() {
  return new PermissionBrokerClientImpl();
}

}  // namespace chromeos
