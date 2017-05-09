// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
#define DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

// Callback that maps a context ID to the NativeView associated with
// that context. This callback is provided to the Device Service by its
// embedder.
using WakeLockContextCallback = base::Callback<gfx::NativeView(int)>;

// Serves requests for WakeLockService connections within a given context.
class WakeLockServiceContext : public mojom::WakeLockContext {
 public:
  WakeLockServiceContext(
      int context_id,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      const WakeLockContextCallback& native_view_getter);
  ~WakeLockServiceContext() override;

  // mojom::WakeLockContext:
  void GetWakeLock(mojom::WakeLockServiceRequest request) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  int context_id_;
  WakeLockContextCallback native_view_getter_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceContext);
};

}  // namespace device

#endif  // DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
