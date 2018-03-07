// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/client_shared_bitmap_manager.h"

#include <stddef.h>

#include <utility>

#include "base/debug/alias.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

namespace {

class ClientSharedBitmap : public SharedBitmap {
 public:
  ClientSharedBitmap(
      scoped_refptr<mojom::ThreadSafeSharedBitmapAllocationNotifierPtr>
          shared_bitmap_allocation_notifier,
      base::SharedMemory* shared_memory,
      const SharedBitmapId& id,
      uint32_t sequence_number)
      : SharedBitmap(static_cast<uint8_t*>(shared_memory->memory()),
                     id,
                     sequence_number),
        shared_bitmap_allocation_notifier_(
            std::move(shared_bitmap_allocation_notifier)),
        tracing_id_(shared_memory->mapped_id()) {}

  ClientSharedBitmap(
      scoped_refptr<mojom::ThreadSafeSharedBitmapAllocationNotifierPtr>
          shared_bitmap_allocation_notifier,
      std::unique_ptr<base::SharedMemory> shared_memory_holder,
      const SharedBitmapId& id,
      uint32_t sequence_number)
      : ClientSharedBitmap(std::move(shared_bitmap_allocation_notifier),
                           shared_memory_holder.get(),
                           id,
                           sequence_number) {
    shared_memory_holder_ = std::move(shared_memory_holder);
  }

  ~ClientSharedBitmap() override {
    (*shared_bitmap_allocation_notifier_)->DidDeleteSharedBitmap(id());
  }

  // SharedBitmap implementation.
  base::UnguessableToken GetCrossProcessGUID() const override {
    return tracing_id_;
  }

 private:
  scoped_refptr<mojom::ThreadSafeSharedBitmapAllocationNotifierPtr>
      shared_bitmap_allocation_notifier_;
  std::unique_ptr<base::SharedMemory> shared_memory_holder_;
  base::UnguessableToken tracing_id_;
};

}  // namespace

ClientSharedBitmapManager::ClientSharedBitmapManager(
    scoped_refptr<mojom::ThreadSafeSharedBitmapAllocationNotifierPtr>
        shared_bitmap_allocation_notifier)
    : shared_bitmap_allocation_notifier_(
          std::move(shared_bitmap_allocation_notifier)) {}

ClientSharedBitmapManager::~ClientSharedBitmapManager() = default;

std::unique_ptr<SharedBitmap> ClientSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size,
    ResourceFormat format) {
  DCHECK(IsBitmapFormatSupported(format));
  TRACE_EVENT2("renderer", "ClientSharedBitmapManager::AllocateSharedBitmap",
               "width", size.width(), "height", size.height());
  SharedBitmapId id = SharedBitmap::GenerateId();
  std::unique_ptr<base::SharedMemory> memory =
      bitmap_allocation::AllocateMappedBitmap(size, format);
  uint32_t sequence_number = NotifyAllocatedSharedBitmap(memory.get(), id);
  return std::make_unique<ClientSharedBitmap>(
      shared_bitmap_allocation_notifier_, std::move(memory), id,
      sequence_number);
}

std::unique_ptr<SharedBitmap> ClientSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    ResourceFormat,
    const SharedBitmapId&) {
  NOTREACHED();
  return nullptr;
}

bool ClientSharedBitmapManager::ChildAllocatedSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const SharedBitmapId& id) {
  // Display compositor only.
  NOTREACHED();
  return false;
}

void ClientSharedBitmapManager::ChildDeletedSharedBitmap(
    const SharedBitmapId& id) {
  // Display compositor only.
  NOTREACHED();
}

std::unique_ptr<SharedBitmap>
ClientSharedBitmapManager::GetBitmapForSharedMemory(base::SharedMemory* mem) {
  SharedBitmapId id = SharedBitmap::GenerateId();
  uint32_t sequence_number = NotifyAllocatedSharedBitmap(mem, id);
  return std::make_unique<ClientSharedBitmap>(
      shared_bitmap_allocation_notifier_, mem, id, sequence_number);
}

// Notifies the browser process that a shared bitmap with the given ID was
// allocated. Caller keeps ownership of |memory|.
uint32_t ClientSharedBitmapManager::NotifyAllocatedSharedBitmap(
    base::SharedMemory* memory,
    const SharedBitmapId& id) {
  mojo::ScopedSharedBufferHandle buffer_handle =
      bitmap_allocation::DuplicateAndCloseMappedBitmap(memory, gfx::Size(),
                                                       RGBA_8888);
  {
    base::AutoLock lock(lock_);
    (*shared_bitmap_allocation_notifier_)
        ->DidAllocateSharedBitmap(std::move(buffer_handle), id);
    return ++last_sequence_number_;
  }
}

}  // namespace viz
