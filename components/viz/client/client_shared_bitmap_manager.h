// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_CLIENT_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_CLIENT_CLIENT_SHARED_BITMAP_MANAGER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "cc/ipc/shared_bitmap_allocation_notifier.mojom.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"

namespace viz {

// A SharedBitmapManager implementation for use outside of the display
// compositor's process. This implementation supports SharedBitmaps that
// can be transported over process boundaries to the display compositor.
class ClientSharedBitmapManager : public cc::SharedBitmapManager {
 public:
  explicit ClientSharedBitmapManager(
      scoped_refptr<
          cc::mojom::ThreadSafeSharedBitmapAllocationNotifierAssociatedPtr>
          shared_bitmap_allocation_notifier);
  ~ClientSharedBitmapManager() override;

  // cc::SharedBitmapManager implementation.
  std::unique_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override;
  std::unique_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const cc::SharedBitmapId&) override;

  std::unique_ptr<cc::SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory* mem);

 private:
  void NotifyAllocatedSharedBitmap(base::SharedMemory* memory,
                                   const cc::SharedBitmapId& id);

  scoped_refptr<
      cc::mojom::ThreadSafeSharedBitmapAllocationNotifierAssociatedPtr>
      shared_bitmap_allocation_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ClientSharedBitmapManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_CLIENT_SHARED_BITMAP_MANAGER_H_
