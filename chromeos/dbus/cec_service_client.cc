// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cec_service_client.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/fake_cec_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Real implementation of CecServiceClient.
class CecServiceClientImpl : public CecServiceClient {
 public:
  CecServiceClientImpl() = default;

  ~CecServiceClientImpl() override = default;

  void SendStandBy() override {
    dbus::MethodCall method_call(cecservice::kCecServiceInterface,
                                 cecservice::kSendStandByToAllDevicesMethod);
    cec_service_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

  void SendWakeUp() override {
    dbus::MethodCall method_call(cecservice::kCecServiceInterface,
                                 cecservice::kSendWakeUpToAllDevicesMethod);
    cec_service_proxy_->CallMethod(&method_call,
                                   dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                   base::DoNothing());
  }

 protected:
  void Init(dbus::Bus* bus) override {
    cec_service_proxy_ =
        bus->GetObjectProxy(cecservice::kCecServiceName,
                            dbus::ObjectPath(cecservice::kCecServicePath));
  }

 private:
  scoped_refptr<dbus::ObjectProxy> cec_service_proxy_;

  DISALLOW_COPY_AND_ASSIGN(CecServiceClientImpl);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// CecServiceClient

CecServiceClient::CecServiceClient() = default;

CecServiceClient::~CecServiceClient() = default;

// static
std::unique_ptr<CecServiceClient> CecServiceClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return std::make_unique<CecServiceClientImpl>();

  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return std::make_unique<FakeCecServiceClient>();
}

}  // namespace chromeos
