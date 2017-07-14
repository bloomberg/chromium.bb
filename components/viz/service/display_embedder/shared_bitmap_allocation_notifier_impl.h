// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SHARED_BITMAP_ALLOCATION_NOTIFIER_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SHARED_BITMAP_ALLOCATION_NOTIFIER_IMPL_H_

#include <unordered_set>

#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "cc/ipc/shared_bitmap_allocation_notifier.mojom.h"
#include "components/viz/common/quads/shared_bitmap.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace viz {
class ServerSharedBitmapManager;

class SharedBitmapAllocationObserver {
 public:
  virtual void DidAllocateSharedBitmap(uint32_t sequence_number) = 0;
};

class VIZ_SERVICE_EXPORT SharedBitmapAllocationNotifierImpl
    : NON_EXPORTED_BASE(public cc::mojom::SharedBitmapAllocationNotifier) {
 public:
  explicit SharedBitmapAllocationNotifierImpl(
      ServerSharedBitmapManager* manager);

  ~SharedBitmapAllocationNotifierImpl() override;

  void AddObserver(SharedBitmapAllocationObserver* observer);
  void RemoveObserver(SharedBitmapAllocationObserver* observer);

  void Bind(cc::mojom::SharedBitmapAllocationNotifierRequest request);

  // cc::mojom::SharedBitmapAllocationNotifier overrides:
  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const SharedBitmapId& id) override;

  void ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  const SharedBitmapId& id);

  void ChildDied();

  uint32_t last_sequence_number() const { return last_sequence_number_; }

 private:
  THREAD_CHECKER(thread_checker_);
  ServerSharedBitmapManager* const manager_;
  mojo::Binding<cc::mojom::SharedBitmapAllocationNotifier> binding_;
  std::unordered_set<SharedBitmapId, SharedBitmapIdHash> owned_bitmaps_;
  base::ObserverList<SharedBitmapAllocationObserver> observers_;
  uint32_t last_sequence_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SharedBitmapAllocationNotifierImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SHARED_BITMAP_ALLOCATION_NOTIFIER_IMPL_H_
