// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_provider.h"

#include <utility>

#include "device/wake_lock/wake_lock_service_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

// static
void WakeLockProvider::Create(
    mojom::WakeLockProviderRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter) {
  mojo::MakeStrongBinding(base::MakeUnique<WakeLockProvider>(
                              std::move(file_task_runner), native_view_getter),
                          std::move(request));
}

WakeLockProvider::WakeLockProvider(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter)
    : file_task_runner_(std::move(file_task_runner)),
      native_view_getter_(native_view_getter) {}

WakeLockProvider::~WakeLockProvider() {}

void WakeLockProvider::GetWakeLockContextForID(
    int context_id,
    mojo::InterfaceRequest<mojom::WakeLockContext> request) {
  DCHECK(context_id >= 0);
  mojo::MakeStrongBinding(
      base::MakeUnique<WakeLockServiceContext>(context_id, file_task_runner_,
                                               native_view_getter_),
      std::move(request));
}

void WakeLockProvider::GetWakeLockWithoutContext(
    mojom::WakeLockType type,
    mojom::WakeLockReason reason,
    const std::string& description,
    mojom::WakeLockServiceRequest request) {
  // WakeLockServiceImpl owns itself.
  new WakeLockServiceImpl(std::move(request), type, reason, description,
                          WakeLockServiceContext::WakeLockInvalidContextId,
                          native_view_getter_, file_task_runner_);
}

}  // namespace device
