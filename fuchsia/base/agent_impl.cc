// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/agent_impl.h"

#include "base/bind.h"

namespace cr_fuchsia {

AgentImpl::ComponentStateBase::~ComponentStateBase() = default;

AgentImpl::ComponentStateBase::ComponentStateBase(
    base::StringPiece component_id)
    : component_id_(component_id) {
  fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
  service_directory_.Initialize(directory.NewRequest());
  service_provider_ = std::make_unique<base::fuchsia::ServiceProviderImpl>(
      std::move(directory));

  // Tear down this instance when the client disconnects from the directory.
  service_provider_->SetOnLastClientDisconnectedClosure(base::BindOnce(
      &ComponentStateBase::OnComponentDisconnected, base::Unretained(this)));
}

void AgentImpl::ComponentStateBase::OnComponentDisconnected() {
  DCHECK(agent_impl_);
  agent_impl_->DeleteComponentState(component_id_);
  // Do not touch |this|, since it is already gone.
}

AgentImpl::AgentImpl(
    base::fuchsia::ServiceDirectory* service_directory,
    CreateComponentStateCallback create_component_state_callback)
    : create_component_state_callback_(
          std::move(create_component_state_callback)),
      agent_binding_(service_directory, this) {
  agent_binding_.SetOnLastClientCallback(base::BindOnce(
      &AgentImpl::MaybeRunOnLastClientCallback, base::Unretained(this)));
}

AgentImpl::~AgentImpl() {
  DCHECK(active_components_.empty());
}

void AgentImpl::Connect(
    std::string requester_url,
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services) {
  auto it = active_components_.find(requester_url);
  if (it == active_components_.end()) {
    std::unique_ptr<ComponentStateBase> component_state =
        create_component_state_callback_.Run(requester_url);
    if (!component_state)
      return;

    auto result =
        active_components_.emplace(requester_url, std::move(component_state));
    it = result.first;
    CHECK(result.second);
    it->second->agent_impl_ = this;
  }
  it->second->service_provider_->AddBinding(std::move(services));
}

void AgentImpl::RunTask(std::string task_id, RunTaskCallback callback) {
  NOTIMPLEMENTED();
}

void AgentImpl::DeleteComponentState(base::StringPiece component_id) {
  size_t removed_components = active_components_.erase(component_id);
  DCHECK_EQ(removed_components, 1u);
  MaybeRunOnLastClientCallback();
}

void AgentImpl::MaybeRunOnLastClientCallback() {
  if (!on_last_client_callback_)
    return;
  if (!active_components_.empty() || agent_binding_.has_clients())
    return;
  std::move(on_last_client_callback_).Run();
}

}  // namespace cr_fuchsia
