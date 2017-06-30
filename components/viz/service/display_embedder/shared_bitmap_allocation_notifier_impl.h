// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SHARED_BITMAP_ALLOCATION_NOTIFIER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SHARED_BITMAP_ALLOCATION_NOTIFIER_IMPL_H_

#include <unordered_set>

#include "cc/ipc/shared_bitmap_allocation_notifier.mojom.h"
#include "cc/resources/shared_bitmap.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace viz {
class ServerSharedBitmapManager;

class VIZ_SERVICE_EXPORT SharedBitmapAllocationNotifierImpl
    : NON_EXPORTED_BASE(public cc::mojom::SharedBitmapAllocationNotifier) {
 public:
  explicit SharedBitmapAllocationNotifierImpl(
      ServerSharedBitmapManager* manager);

  ~SharedBitmapAllocationNotifierImpl() override;

  void Bind(cc::mojom::SharedBitmapAllocationNotifierAssociatedRequest request);

  // cc::mojom::SharedBitmapAllocationNotifier overrides:
  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const cc::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const cc::SharedBitmapId& id) override;

  void ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  const cc::SharedBitmapId& id);

 private:
  ServerSharedBitmapManager* const manager_;
  mojo::AssociatedBinding<cc::mojom::SharedBitmapAllocationNotifier> binding_;
  std::unordered_set<cc::SharedBitmapId, cc::SharedBitmapIdHash> owned_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(SharedBitmapAllocationNotifierImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SHARED_BITMAP_ALLOCATION_NOTIFIER_IMPL_H_
