// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/wilco_dtc_supportd_client.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void OnVoidDBusMethod(VoidDBusMethodCallback callback,
                      dbus::Response* response) {
  std::move(callback).Run(response != nullptr);
}

// The WilcoDtcSupportdClient implementation used in production.
class WilcoDtcSupportdClientImpl final : public WilcoDtcSupportdClient {
 public:
  WilcoDtcSupportdClientImpl();
  ~WilcoDtcSupportdClientImpl() override;

  // WilcoDtcSupportdClient overrides:
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(base::ScopedFD fd,
                               VoidDBusMethodCallback callback) override;

 protected:
  // WilcoDtcSupportdClient overrides:
  void Init(dbus::Bus* bus) override;

 private:
  dbus::ObjectProxy* wilco_dtc_supportd_proxy_ = nullptr;

  base::WeakPtrFactory<WilcoDtcSupportdClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WilcoDtcSupportdClientImpl);
};

WilcoDtcSupportdClientImpl::WilcoDtcSupportdClientImpl() = default;

WilcoDtcSupportdClientImpl::~WilcoDtcSupportdClientImpl() = default;

void WilcoDtcSupportdClientImpl::WaitForServiceToBeAvailable(
    WaitForServiceToBeAvailableCallback callback) {
  wilco_dtc_supportd_proxy_->WaitForServiceToBeAvailable(std::move(callback));
}

void WilcoDtcSupportdClientImpl::BootstrapMojoConnection(
    base::ScopedFD fd,
    VoidDBusMethodCallback callback) {
  dbus::MethodCall method_call(
      ::diagnostics::kWilcoDtcSupportdServiceInterface,
      ::diagnostics::kWilcoDtcSupportdBootstrapMojoConnectionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendFileDescriptor(fd.get());
  wilco_dtc_supportd_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
      base::BindOnce(&OnVoidDBusMethod, std::move(callback)));
}

void WilcoDtcSupportdClientImpl::Init(dbus::Bus* bus) {
  wilco_dtc_supportd_proxy_ = bus->GetObjectProxy(
      ::diagnostics::kWilcoDtcSupportdServiceName,
      dbus::ObjectPath(::diagnostics::kWilcoDtcSupportdServicePath));
}

}  // namespace

// static
std::unique_ptr<WilcoDtcSupportdClient> WilcoDtcSupportdClient::Create() {
  return std::make_unique<WilcoDtcSupportdClientImpl>();
}

WilcoDtcSupportdClient::WilcoDtcSupportdClient() = default;

}  // namespace chromeos
