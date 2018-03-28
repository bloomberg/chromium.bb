// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/concierge_client.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/vm_concierge/dbus-constants.h"

namespace chromeos {

class ConciergeClientImpl : public ConciergeClient {
 public:
  ConciergeClientImpl() : weak_ptr_factory_(this) {}

  ~ConciergeClientImpl() override = default;

  void CreateDiskImage(
      const vm_tools::concierge::CreateDiskImageRequest& request,
      DBusMethodCallback<vm_tools::concierge::CreateDiskImageResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kCreateDiskImageMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode CreateDiskImageRequest protobuf";
      std::move(callback).Run(base::nullopt);
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::CreateDiskImageResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartTerminaVm(const vm_tools::concierge::StartVmRequest& request,
                      DBusMethodCallback<vm_tools::concierge::StartVmResponse>
                          callback) override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStartVmMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartVmRequest protobuf";
      std::move(callback).Run(base::nullopt);
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::StartVmResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StopVm(const vm_tools::concierge::StopVmRequest& request,
              DBusMethodCallback<vm_tools::concierge::StopVmResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStopVmMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StopVmRequest protobuf";
      std::move(callback).Run(base::nullopt);
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::StopVmResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void StartContainer(
      const vm_tools::concierge::StartContainerRequest& request,
      DBusMethodCallback<vm_tools::concierge::StartContainerResponse> callback)
      override {
    dbus::MethodCall method_call(vm_tools::concierge::kVmConciergeInterface,
                                 vm_tools::concierge::kStartContainerMethod);
    dbus::MessageWriter writer(&method_call);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode StartContainerRequest protobuf";
      std::move(callback).Run(base::nullopt);
      return;
    }

    concierge_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_INFINITE,
        base::BindOnce(&ConciergeClientImpl::OnDBusProtoResponse<
                           vm_tools::concierge::StartContainerResponse>,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    concierge_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

 protected:
  void Init(dbus::Bus* bus) override {
    concierge_proxy_ = bus->GetObjectProxy(
        vm_tools::concierge::kVmConciergeServiceName,
        dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath));
    if (!concierge_proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << vm_tools::concierge::kVmConciergeServiceName;
    }
  }

 private:
  template <typename ResponseProto>
  void OnDBusProtoResponse(DBusMethodCallback<ResponseProto> callback,
                           dbus::Response* dbus_response) {
    if (!dbus_response) {
      std::move(callback).Run(base::nullopt);
      return;
    }
    ResponseProto reponse_proto;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&reponse_proto)) {
      LOG(ERROR) << "Failed to parse proto from DBus response.";
      std::move(callback).Run(base::nullopt);
      return;
    }
    std::move(callback).Run(std::move(reponse_proto));
  }

  dbus::ObjectProxy* concierge_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ConciergeClientImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ConciergeClientImpl);
};

ConciergeClient::ConciergeClient() = default;

ConciergeClient::~ConciergeClient() = default;

ConciergeClient* ConciergeClient::Create() {
  return new ConciergeClientImpl();
}

}  // namespace chromeos
