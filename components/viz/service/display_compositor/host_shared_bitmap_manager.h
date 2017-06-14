// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_HOST_SHARED_BITMAP_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_HOST_SHARED_BITMAP_MANAGER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>

#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/ipc/shared_bitmap_manager.mojom.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<cc::SharedBitmapId> {
  size_t operator()(const cc::SharedBitmapId& id) const {
    return base::Hash(reinterpret_cast<const char*>(id.name), sizeof(id.name));
  }
};
}  // namespace BASE_HASH_NAMESPACE

namespace viz {
class BitmapData;
class HostSharedBitmapManager;

class VIZ_SERVICE_EXPORT HostSharedBitmapManagerClient
    : NON_EXPORTED_BASE(public cc::mojom::SharedBitmapManager) {
 public:
  explicit HostSharedBitmapManagerClient(HostSharedBitmapManager* manager);

  ~HostSharedBitmapManagerClient() override;

  void Bind(cc::mojom::SharedBitmapManagerAssociatedRequest request);

  // cc::mojom::SharedBitmapManager overrides:
  void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const cc::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const cc::SharedBitmapId& id) override;

  void ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  const cc::SharedBitmapId& id);

 private:
  HostSharedBitmapManager* manager_;
  mojo::AssociatedBinding<cc::mojom::SharedBitmapManager> binding_;

  // Lock must be held around access to owned_bitmaps_.
  base::Lock lock_;
  base::hash_set<cc::SharedBitmapId> owned_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(HostSharedBitmapManagerClient);
};

class VIZ_SERVICE_EXPORT HostSharedBitmapManager
    : public cc::SharedBitmapManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  HostSharedBitmapManager();
  ~HostSharedBitmapManager() override;

  static HostSharedBitmapManager* current();

  // cc::SharedBitmapManager implementation.
  std::unique_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override;
  std::unique_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size& size,
      const cc::SharedBitmapId&) override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  size_t AllocatedBitmapCount() const;

  void FreeSharedMemoryFromMap(const cc::SharedBitmapId& id);

 private:
  friend class HostSharedBitmapManagerClient;

  bool ChildAllocatedSharedBitmap(size_t buffer_size,
                                  const base::SharedMemoryHandle& handle,
                                  const cc::SharedBitmapId& id);
  void ChildDeletedSharedBitmap(const cc::SharedBitmapId& id);

  mutable base::Lock lock_;

  typedef base::hash_map<cc::SharedBitmapId, scoped_refptr<BitmapData>>
      BitmapMap;
  BitmapMap handle_map_;

  DISALLOW_COPY_AND_ASSIGN(HostSharedBitmapManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_COMPOSITOR_HOST_SHARED_BITMAP_MANAGER_H_
