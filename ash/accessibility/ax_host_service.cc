// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ax_host_service.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_delegate.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "ui/accessibility/ax_event.h"
#include "ui/views/mus/ax_remote_host.h"

namespace ash {

AXHostService* AXHostService::instance_ = nullptr;

bool AXHostService::automation_enabled_ = false;

AXHostService::AXHostService() {
  // AX tree ID is automatically assigned.
  DCHECK(!tree_id().empty());

  // TODO(jamescook): Eliminate this when multiple remote trees are supported.
  Shell::Get()->accessibility_controller()->set_remote_ax_tree_id(tree_id());

  DCHECK(!instance_);
  instance_ = this;
  registry_.AddInterface<ax::mojom::AXHost>(
      base::BindRepeating(&AXHostService::AddBinding, base::Unretained(this)));
}

AXHostService::~AXHostService() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
void AXHostService::SetAutomationEnabled(bool enabled) {
  automation_enabled_ = enabled;
  if (instance_)
    instance_->NotifyAutomationEnabled();
}

void AXHostService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void AXHostService::SetRemoteHost(ax::mojom::AXRemoteHostPtr remote,
                                  SetRemoteHostCallback cb) {
  remote_host_ = std::move(remote);

  // Handle both clean and unclean shutdown.
  remote_host_.set_connection_error_handler(base::BindOnce(
      &AXHostService::OnRemoteHostDisconnected, base::Unretained(this)));

  std::move(cb).Run(tree_id(), automation_enabled_);
}

void AXHostService::HandleAccessibilityEvent(
    const std::string& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const ui::AXEvent& event) {
  CHECK_EQ(tree_id, this->tree_id());
  // Use an in-process delegate back to Chrome for efficiency in classic ash.
  // For mash we'll need to pass around a mojo interface pointer such that a
  // remote app can talk directly to the browser process and not ping-pong
  // through the ash process.
  Shell::Get()->accessibility_delegate()->DispatchAccessibilityEvent(
      tree_id, updates, event);
}

void AXHostService::PerformAction(const ui::AXActionData& data) {
  // TODO(jamescook): This assumes a single remote host. Need to have one
  // AXHostDelegate per remote host and only send to the appropriate one.
  if (remote_host_)
    remote_host_->PerformAction(data);
}

void AXHostService::FlushForTesting() {
  remote_host_.FlushForTesting();
}

void AXHostService::AddBinding(ax::mojom::AXHostRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AXHostService::NotifyAutomationEnabled() {
  if (remote_host_)
    remote_host_->OnAutomationEnabled(automation_enabled_);
}

void AXHostService::OnRemoteHostDisconnected() {
  Shell::Get()->accessibility_delegate()->DispatchTreeDestroyedEvent(tree_id());
}

}  // namespace ash
