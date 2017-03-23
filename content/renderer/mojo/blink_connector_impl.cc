// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_connector_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

BlinkConnectorImpl::BlinkConnectorImpl(
    std::unique_ptr<service_manager::Connector> connector)
    : connector_(std::move(connector)),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

BlinkConnectorImpl::~BlinkConnectorImpl() = default;

void BlinkConnectorImpl::bindInterface(const char* service_name,
                                       const char* interface_name,
                                       mojo::ScopedMessagePipeHandle handle) {
  // |connector_| is null in some testing contexts.
  if (!connector_)
    return;

  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BlinkConnectorImpl::bindInterface, GetWeakPtr(),
                   service_name, interface_name, base::Passed(&handle)));
    return;
  }

  // Tests might have overridden this interface with a local implementation.
  InterfaceBinderMap* overrides_for_service =
      GetOverridesForService(service_name);
  if (overrides_for_service) {
    auto it = overrides_for_service->find(std::string(interface_name));
    if (it != overrides_for_service->end()) {
      it->second.Run(std::move(handle));
      return;
    }
  }

  service_manager::Identity service_identity(
      service_name, service_manager::mojom::kInheritUserID);
  connector_->BindInterface(service_identity, interface_name,
                            std::move(handle));
}

void BlinkConnectorImpl::AddOverrideForTesting(
    const std::string& service_name,
    const std::string& interface_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>& binder) {
  if (service_binders_.find(service_name) == service_binders_.end())
    service_binders_[service_name] = base::MakeUnique<InterfaceBinderMap>();

  (*(service_binders_[service_name]))[interface_name] = binder;
}

void BlinkConnectorImpl::ClearOverridesForTesting() {
  service_binders_.clear();
}

base::WeakPtr<BlinkConnectorImpl> BlinkConnectorImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BlinkConnectorImpl::InterfaceBinderMap*
BlinkConnectorImpl::GetOverridesForService(const char* service_name) {
  // Short-circuit out in the case where there are no overrides (always true in
  // production).
  if (service_binders_.empty())
    return nullptr;

  auto it = service_binders_.find(std::string(service_name));

  if (it != service_binders_.end())
    return it->second.get();

  return nullptr;
}

}  // namespace content
