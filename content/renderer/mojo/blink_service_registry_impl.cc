// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_service_registry_impl.h"

#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/mojo/service_registry_impl.h"

namespace content {

BlinkServiceRegistryImpl::BlinkServiceRegistryImpl(
    base::WeakPtr<content::ServiceRegistry> service_registry)
    : service_registry_(service_registry),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

BlinkServiceRegistryImpl::~BlinkServiceRegistryImpl() = default;

void BlinkServiceRegistryImpl::connectToRemoteService(
    const char* name,
    mojo::ScopedMessagePipeHandle handle) {
  if (!main_thread_task_runner_->BelongsToCurrentThread()) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&BlinkServiceRegistryImpl::connectToRemoteService,
                              weak_ptr_factory_.GetWeakPtr(), name,
                               base::Passed(&handle)));
    return;
  }

  if (!service_registry_)
    return;

  service_registry_->ConnectToRemoteService(name, std::move(handle));
}

}  // namespace content
