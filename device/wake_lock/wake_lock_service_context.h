// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
#define DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_

#include <memory>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#include "device/wake_lock/wake_lock_service_impl.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

class PowerSaveBlocker;

// Callback that maps a context ID to the NativeView associated with
// that context. This callback is provided to the Device Service by its
// embedder.
using WakeLockContextCallback = base::Callback<gfx::NativeView(int)>;

// Serves requests for WakeLockService connections within a given context.

// Note that the lifetime model of WakeLockContext is somewhat complex: It must
// stay alive as long as either
// (1) Its Mojo connection is still valid (as the client might make future
// GetWakeLock() calls) OR
// (2) There are still live WakeLock instances that it has instantiated (since
// they call into it when they receive Mojo requests from *their* clients).
// Consequently, WakeLockContext monitors the state of the connections described
// in (1) and (2), dying only when *all* of those connections go away.
class WakeLockServiceContext : public mojom::WakeLockContext {
 public:
  WakeLockServiceContext(
      mojom::WakeLockContextRequest request,
      int context_id,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      const WakeLockContextCallback& native_view_getter);
  ~WakeLockServiceContext() override;

  // mojom::WakeLockContext:
  void GetWakeLock(
      mojo::InterfaceRequest<mojom::WakeLockService> request) override;
  void HasWakeLockForTests(
      const HasWakeLockForTestsCallback& callback) override;

  // Requests wake lock.
  void RequestWakeLock();

  // Cancels pending wake lock request.
  void CancelWakeLock();

 private:
  void CreateWakeLock();
  void RemoveWakeLock();
  void UpdateWakeLock();
  void OnContextBindingError();

  // Checks whether this instance is still needed, and if not, destroys it.
  void DestroyIfNoLongerNeeded();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  int num_lock_requests_;

  // The actual power save blocker for screen.
  std::unique_ptr<PowerSaveBlocker> wake_lock_;

#if defined(OS_ANDROID)
  int context_id_;
  WakeLockContextCallback native_view_getter_;
#endif

  mojo::Binding<mojom::WakeLockContext> context_binding_;
  bool context_binding_encountered_error_;

  mojo::StrongBindingSet<mojom::WakeLockService> wake_lock_bindings_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceContext);
};

}  // namespace device

#endif  // DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
