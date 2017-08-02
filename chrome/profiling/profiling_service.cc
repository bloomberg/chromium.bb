// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profiling/profiling_service.h"

#include "chrome/profiling/memlog_impl.h"

namespace profiling {

ProfilingService::ProfilingService() : weak_factory_(this) {}

ProfilingService::~ProfilingService() {}

std::unique_ptr<service_manager::Service> ProfilingService::CreateService() {
  return base::MakeUnique<ProfilingService>();
}

void ProfilingService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(base::Bind(
      &ProfilingService::MaybeRequestQuitDelayed, base::Unretained(this))));
  registry_.AddInterface(base::Bind(&ProfilingService::OnMemlogRequest,
                                    base::Unretained(this),
                                    ref_factory_.get()));
  memlog_impl_ = base::MakeUnique<MemlogImpl>();
}

void ProfilingService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));

  // TODO(ajwong): Maybe signal shutdown when all interfaces are closed?  What
  // does ServiceManager actually do?
}

void ProfilingService::MaybeRequestQuitDelayed() {
  // TODO(ajwong): What does this and the MaybeRequestQuit() function actually
  // do? This is just cargo-culted from another mojo service.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ProfilingService::MaybeRequestQuit,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(5));
}

void ProfilingService::MaybeRequestQuit() {
  DCHECK(ref_factory_);
  if (ref_factory_->HasNoRefs())
    context()->RequestQuit();
}

void ProfilingService::OnMemlogRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    mojom::MemlogRequest request) {
  binding_set_.AddBinding(memlog_impl_.get(), std::move(request));
}

}  // namespace profiling
