// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
#define CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_

#include <memory>
#include <set>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/browser/wake_lock/wake_lock_service_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "ui/gfx/native_widget_types.h"

namespace device {
class PowerSaveBlocker;
}  // namespace device

namespace content {

class CONTENT_EXPORT WakeLockServiceContext {
 public:
  WakeLockServiceContext(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      base::Callback<gfx::NativeView()> native_view_getter);
  ~WakeLockServiceContext();

  // Creates a WakeLockServiceImpl that is strongly bound to |request|.
  void CreateService(
      mojo::InterfaceRequest<blink::mojom::WakeLockService> request);

  // Requests wake lock.
  void RequestWakeLock();

  // Cancels pending wake lock request.
  void CancelWakeLock();

  // Used by tests.
  bool HasWakeLockForTests() const;

 private:
  void CreateWakeLock();
  void RemoveWakeLock();
  void UpdateWakeLock();

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  int num_lock_requests_;

  // The actual power save blocker for screen.
  std::unique_ptr<device::PowerSaveBlocker> wake_lock_;
  base::Callback<gfx::NativeView()> native_view_getter_;

  base::WeakPtrFactory<WakeLockServiceContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WakeLockServiceContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WAKE_LOCK_WAKE_LOCK_SERVICE_CONTEXT_H_
