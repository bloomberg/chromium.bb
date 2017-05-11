// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_PROVIDER_H_
#define DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_PROVIDER_H_

#include "base/sequenced_task_runner.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#include "device/wake_lock/public/interfaces/wake_lock_context_provider.mojom.h"
#include "device/wake_lock/wake_lock_service_context.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

// Serves requests for WakeLockContext connections.
class WakeLockContextProvider : public mojom::WakeLockContextProvider {
 public:
  WakeLockContextProvider(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      const WakeLockContextCallback& native_view_getter);
  ~WakeLockContextProvider() override;

  static void Create(
      mojom::WakeLockContextProviderRequest request,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      const WakeLockContextCallback& native_view_getter);

  // mojom::WakeLockContextProvider:
  void GetWakeLockContextForID(
      int context_id,
      mojo::InterfaceRequest<mojom::WakeLockContext> request) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  WakeLockContextCallback native_view_getter_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockContextProvider);
};

}  // namespace device

#endif  // DEVICE_WAKE_LOCK_WAKE_LOCK_CONTEXT_PROVIDER_H_
