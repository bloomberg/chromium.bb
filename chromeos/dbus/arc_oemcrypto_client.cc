// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/arc_oemcrypto_client.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class ArcOemCryptoClientImpl : public ArcOemCryptoClient {
 public:
  ArcOemCryptoClientImpl() : weak_ptr_factory_(this) {}
  ~ArcOemCryptoClientImpl() override {}

  // ArcOemCryptoClient override:
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(arc_oemcrypto::kArcOemCryptoServiceInterface,
                                 arc_oemcrypto::kBootstrapMojoConnection);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.release());
    proxy_->CallMethod(&method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                       base::Bind(&ArcOemCryptoClientImpl::OnVoidDBusMethod,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  base::Passed(std::move(callback))));
  }

 protected:
  // DBusClient override.
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(
        arc_oemcrypto::kArcOemCryptoServiceName,
        dbus::ObjectPath(arc_oemcrypto::kArcOemCryptoServicePath));
  }

 private:
  // Runs the callback with the method call result.
  void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                        dbus::Response* response) {
    std::move(callback).Run(response ? DBUS_METHOD_CALL_SUCCESS
                                     : DBUS_METHOD_CALL_FAILURE);
  }

  dbus::ObjectProxy* proxy_ = nullptr;

  base::WeakPtrFactory<ArcOemCryptoClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcOemCryptoClientImpl);
};

}  // namespace

ArcOemCryptoClient::ArcOemCryptoClient() {}

ArcOemCryptoClient::~ArcOemCryptoClient() {}

// static
ArcOemCryptoClient* ArcOemCryptoClient::Create() {
  return new ArcOemCryptoClientImpl();
}

}  // namespace chromeos
