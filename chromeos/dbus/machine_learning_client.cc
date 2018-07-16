// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/machine_learning_client.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/fake_machine_learning_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class MachineLearningClientImpl : public MachineLearningClient {
 public:
  MachineLearningClientImpl() : weak_ptr_factory_(this) {}

  // MachineLearningClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override {
    ml_service_proxy_->WaitForServiceToBeAvailable(
        base::BindOnce(&MachineLearningClientImpl::OnServiceAvailable,
                       weak_ptr_factory_.GetWeakPtr(), std::move(fd),
                       std::move(result_callback)));
  }

 protected:
  // DBusClient:
  void Init(dbus::Bus* const bus) override {
    ml_service_proxy_ = bus->GetObjectProxy(
        ml::kMlServiceName, dbus::ObjectPath(ml::kMlServicePath));
  }

 private:
  dbus::ObjectProxy* ml_service_proxy_ = nullptr;

  // Actually sends the Mojo-bootstrap message to the ML service daemon, once
  // the daemon's D-Bus interface is available.
  void OnServiceAvailable(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback,
      const bool service_is_available) {
    // Return failure immediately if D-Bus service is not available.
    if (!service_is_available) {
      const bool success = false;
      std::move(result_callback).Run(success);
      return;
    }

    // Call the bootstrap D-Bus method.
    dbus::MethodCall method_call(ml::kMlServiceName,
                                 ml::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendFileDescriptor(fd.get());
    ml_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &MachineLearningClientImpl::OnBootstrapMojoConnectionResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
  }

  // Passes the success/failure of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      base::OnceCallback<void(bool success)> result_callback,
      dbus::Response* const dbus_response) {
    const bool success = dbus_response != nullptr;
    std::move(result_callback).Run(success);
  }

  // Must be last class member.
  base::WeakPtrFactory<MachineLearningClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MachineLearningClientImpl);
};

}  // namespace

// static
std::unique_ptr<MachineLearningClient> MachineLearningClient::Create(
    DBusClientImplementationType type) {
  if (type == REAL_DBUS_CLIENT_IMPLEMENTATION)
    return std::make_unique<MachineLearningClientImpl>();
  DCHECK_EQ(FAKE_DBUS_CLIENT_IMPLEMENTATION, type);
  return std::make_unique<FakeMachineLearningClient>();
}

}  // namespace chromeos
