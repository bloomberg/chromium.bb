// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
#define DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_

#include "base/macros.h"
#include "device/wake_lock/public/interfaces/wake_lock_service.mojom.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace device {

class WakeLockServiceContext;

class WakeLockServiceImpl : public mojom::WakeLockService {
 public:
  explicit WakeLockServiceImpl(WakeLockServiceContext* context);
  ~WakeLockServiceImpl() override;

  // WakeLockSevice implementation.
  void RequestWakeLock() override;
  void CancelWakeLock() override;

 private:
  // Will outlive this instance.
  WakeLockServiceContext* context_;
  bool wake_lock_request_outstanding_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceImpl);
};

}  // namespace device

#endif  // DEVICE_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
