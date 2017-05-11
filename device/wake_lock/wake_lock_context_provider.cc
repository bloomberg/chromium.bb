// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/wake_lock/wake_lock_context_provider.h"

#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

// static
void WakeLockContextProvider::Create(
    mojom::WakeLockContextProviderRequest request,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter) {
  mojo::MakeStrongBinding(base::MakeUnique<WakeLockContextProvider>(
                              std::move(file_task_runner), native_view_getter),
                          std::move(request));
}

WakeLockContextProvider::WakeLockContextProvider(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    const WakeLockContextCallback& native_view_getter)
    : file_task_runner_(std::move(file_task_runner)),
      native_view_getter_(native_view_getter) {}

WakeLockContextProvider::~WakeLockContextProvider() {}

void WakeLockContextProvider::GetWakeLockContextForID(
    int context_id,
    mojo::InterfaceRequest<mojom::WakeLockContext> request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<WakeLockServiceContext>(context_id, file_task_runner_,
                                               native_view_getter_),
      std::move(request));
}

}  // namespace device
