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
#include "base/synchronization/lock.h"
#include "components/viz/client/viz_client_export.h"
#include "components/viz/common/resources/shared_bitmap_manager.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"
#include "services/viz/public/interfaces/compositing/shared_bitmap_allocation_notifier.mojom.h"

namespace viz {

// A SharedBitmapManager implementation for use outside of the display
// compositor's process. This implementation supports SharedBitmaps that
// can be transported over process boundaries to the display compositor.
class VIZ_CLIENT_EXPORT ClientSharedBitmapManager : public SharedBitmapManager {
 public:
  explicit ClientSharedBitmapManager(
      scoped_refptr<mojom::ThreadSafeSharedBitmapAllocationNotifierPtr>
          shared_bitmap_allocation_notifier);
  ~ClientSharedBitmapManager() override;

  // SharedBitmapManager implementation.
  std::unique_ptr<SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override;
  std::unique_ptr<SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const SharedBitmapId&) override;

  std::unique_ptr<SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory* mem);

 private:
  uint32_t NotifyAllocatedSharedBitmap(base::SharedMemory* memory,
                                       const SharedBitmapId& id);

  scoped_refptr<mojom::ThreadSafeSharedBitmapAllocationNotifierPtr>
      shared_bitmap_allocation_notifier_;

  base::Lock lock_;

  // Each SharedBitmap allocated by this class is assigned a unique sequence
  // number that is incremental.
  uint32_t last_sequence_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ClientSharedBitmapManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_CLIENT_SHARED_BITMAP_MANAGER_H_
