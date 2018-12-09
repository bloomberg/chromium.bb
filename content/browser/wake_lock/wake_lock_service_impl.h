// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_

#include "content/public/browser/frame_service_base.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom.h"

namespace content {

class WakeLockServiceImpl final
    : public FrameServiceBase<blink::mojom::WakeLockService> {
 public:
  static void Create(RenderFrameHost*, blink::mojom::WakeLockServiceRequest);

  // WakeLockService implementation.
  void GetWakeLock(device::mojom::WakeLockType,
                   device::mojom::WakeLockReason,
                   const std::string&,
                   device::mojom::WakeLockRequest) final;

 private:
  WakeLockServiceImpl(RenderFrameHost*, blink::mojom::WakeLockServiceRequest);

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_IMPL_H_
