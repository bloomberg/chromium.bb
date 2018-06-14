// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_service.h"

#include "ash/mojo_interface_factory.h"
#include "ash/shell.h"
#include "ash/ws/window_service_owner.h"
#include "base/bind.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "services/service_manager/embedder/embedded_service_info.h"
#include "services/ui/public/interfaces/constants.mojom.h"

namespace ash {
namespace {

std::unique_ptr<service_manager::Service> CreateAshService() {
  return std::make_unique<AshService>();
}

}  // namespace

AshService::AshService() = default;

AshService::~AshService() = default;

// static
service_manager::EmbeddedServiceInfo AshService::CreateEmbeddedServiceInfo() {
  service_manager::EmbeddedServiceInfo info;
  info.factory = base::BindRepeating(&CreateAshService);
  info.task_runner = base::ThreadTaskRunnerHandle::Get();
  return info;
}

void AshService::OnStart() {
  mojo_interface_factory::RegisterInterfaces(
      &registry_, base::ThreadTaskRunnerHandle::Get());
  registry_.AddInterface(base::BindRepeating(&AshService::BindServiceFactory,
                                             base::Unretained(this)));
}

void AshService::OnBindInterface(
    const service_manager::BindSourceInfo& remote_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle handle) {
  registry_.BindInterface(interface_name, std::move(handle));
}

void AshService::CreateService(
    service_manager::mojom::ServiceRequest service,
    const std::string& name,
    service_manager::mojom::PIDReceiverPtr pid_receiver) {
  DCHECK_EQ(name, ui::mojom::kServiceName);
  Shell::Get()->window_service_owner()->BindWindowService(std::move(service));
  pid_receiver->SetPID(base::GetCurrentProcId());
}

void AshService::BindServiceFactory(
    service_manager::mojom::ServiceFactoryRequest request) {
  service_factory_bindings_.AddBinding(this, std::move(request));
}

}  // namespace ash
