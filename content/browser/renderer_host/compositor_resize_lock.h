// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_RESIZE_LOCK_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_RESIZE_LOCK_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class CompositorResizeLockClient {
 public:
  virtual ~CompositorResizeLockClient() {}

  // Creates and returns a CompositorLock for the CompositoResizeLock to
  // hold.
  virtual std::unique_ptr<ui::CompositorLock> GetCompositorLock(
      ui::CompositorLockClient* client) = 0;

  // Called when the CompositorResizeLock ends. This can happen
  // before the CompositorResizeLock is destroyed if it times out.
  virtual void CompositorResizeLockEnded() = 0;
};

// Used to prevent further resizes while a resize is pending.
class CONTENT_EXPORT CompositorResizeLock : public ui::CompositorLockClient {
 public:
  CompositorResizeLock(CompositorResizeLockClient* client,
                       const gfx::Size& new_size);
  ~CompositorResizeLock() override;

  // Returns |true| if the call locks the compositor, or false if it was ever
  // locked/unlocked.
  bool Lock();

  // Releases the lock on the compositor without releasing the whole resize
  // lock. The client is not told about this. If called before locking, it will
  // prevent locking from happening.
  void UnlockCompositor();

  bool timed_out() const { return timed_out_; }

  const gfx::Size& expected_size() const { return expected_size_; }

 private:
  // ui::CompositorLockClient implementation.
  void CompositorLockTimedOut() override;

  CompositorResizeLockClient* client_;
  const gfx::Size expected_size_;
  std::unique_ptr<ui::CompositorLock> compositor_lock_;
  bool unlocked_ = false;
  bool timed_out_ = false;
  const base::TimeTicks acquisition_time_;

  DISALLOW_COPY_AND_ASSIGN(CompositorResizeLock);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITOR_RESIZE_LOCK_AURA_H_
