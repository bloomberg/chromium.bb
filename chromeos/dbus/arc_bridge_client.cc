// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chromeos/dbus/arc_bridge_client.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// todo(denniskempin): Move constants to the chromiumos platform
// service_constants.h
const char kArcBridgeServicePath[] = "/org/chromium/arc";
const char kArcBridgeServiceName[] = "org.chromium.arc";
const char kArcBridgeServiceInterface[] = "org.chromium.arc";

const char kPingMethod[] = "Ping";

void OnVoidDBusMethod(const VoidDBusMethodCallback& callback,
                      dbus::Response* response) {
  callback.Run(response ? DBUS_METHOD_CALL_SUCCESS : DBUS_METHOD_CALL_FAILURE);
}

class ArcBridgeClientImpl : public ArcBridgeClient {
 public:
  ArcBridgeClientImpl() : proxy_(nullptr), weak_ptr_factory_(this) {}

  ~ArcBridgeClientImpl() override {}

  // Calls Ping method.  |callback| is called after the method call succeeds.
  void Ping(const VoidDBusMethodCallback& callback) override {
    DCHECK(proxy_);

    dbus::MethodCall method_call(kArcBridgeServiceInterface, kPingMethod);
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&OnVoidDBusMethod, callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(kArcBridgeServiceName,
                                 dbus::ObjectPath(kArcBridgeServicePath));
    DCHECK(proxy_);
  }

 private:
  dbus::ObjectProxy* proxy_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ArcBridgeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcBridgeClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ArcBridgeClient

ArcBridgeClient::ArcBridgeClient() {}

ArcBridgeClient::~ArcBridgeClient() {}

// static
ArcBridgeClient* ArcBridgeClient::Create() {
  return new ArcBridgeClientImpl();
}

}  // namespace chromeos
