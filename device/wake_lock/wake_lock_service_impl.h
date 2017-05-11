// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
#define DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "device/power_save_blocker/power_save_blocker.h"
#include "device/wake_lock/public/interfaces/wake_lock_context.mojom.h"
#include "device/wake_lock/public/interfaces/wake_lock_service.mojom.h"
#include "device/wake_lock/wake_lock_service_context.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/native_widget_types.h"

namespace device {

class WakeLockServiceImpl : public mojom::WakeLockService {
 public:
  WakeLockServiceImpl(
      mojom::WakeLockServiceRequest request,
      mojom::WakeLockType type,
      mojom::WakeLockReason reason,
      const std::string& description,
      int context_id,
      WakeLockContextCallback native_view_getter,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
  ~WakeLockServiceImpl() override;

  // WakeLockSevice implementation.
  void RequestWakeLock() override;
  void CancelWakeLock() override;
  void AddClient(mojom::WakeLockServiceRequest request) override;
  void HasWakeLockForTests(
      const HasWakeLockForTestsCallback& callback) override;

 private:
  void UpdateWakeLock();
  void CreateWakeLock();
  void RemoveWakeLock();
  void OnConnectionError();

  mojom::WakeLockType type_;
  mojom::WakeLockReason reason_;
  std::unique_ptr<std::string> description_;
  int num_lock_requests_;

#if defined(OS_ANDROID)
  int context_id_;
  WakeLockContextCallback native_view_getter_;
#endif

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // The actual power save blocker for screen.
  std::unique_ptr<PowerSaveBlocker> wake_lock_;

  // Multiple clients that associate to the same WebContents share the same one
  // WakeLockServiceImpl instance. Two consecutive |RequestWakeLock| requests
  // from the same client should be coalesced as one request. Everytime a new
  // client is being added into the BindingSet, we create an unique_ptr<bool>
  // as its context, which records this client's request status.
  mojo::BindingSet<mojom::WakeLockService, std::unique_ptr<bool>> binding_set_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceImpl);
};

}  // namespace device

#endif  // DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
