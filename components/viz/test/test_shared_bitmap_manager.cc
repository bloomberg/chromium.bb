// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_shared_bitmap_manager.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"

namespace viz {

namespace {

static uint32_t g_next_sequence_number = 1;

class OwnedSharedBitmap : public SharedBitmap {
 public:
  OwnedSharedBitmap(std::unique_ptr<base::SharedMemory> shared_memory,
                    const SharedBitmapId& id)
      : SharedBitmap(static_cast<uint8_t*>(shared_memory->memory()),
                     id,
                     g_next_sequence_number++),
        shared_memory_(std::move(shared_memory)) {}

  ~OwnedSharedBitmap() override = default;

  // SharedBitmap:
  base::SharedMemoryHandle GetSharedMemoryHandle() const override {
    return shared_memory_->handle();
  }

 private:
  std::unique_ptr<base::SharedMemory> shared_memory_;
};

class UnownedSharedBitmap : public SharedBitmap {
 public:
  UnownedSharedBitmap(uint8_t* pixels, const SharedBitmapId& id)
      : SharedBitmap(pixels, id, g_next_sequence_number++) {}

  // SharedBitmap:
  base::SharedMemoryHandle GetSharedMemoryHandle() const override {
    return base::SharedMemoryHandle();
  }
};

}  // namespace

TestSharedBitmapManager::TestSharedBitmapManager() = default;

TestSharedBitmapManager::~TestSharedBitmapManager() = default;

std::unique_ptr<SharedBitmap> TestSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  std::unique_ptr<base::SharedMemory> memory(new base::SharedMemory);
  memory->CreateAndMapAnonymous(size.GetArea() * 4);
  SharedBitmapId id = SharedBitmap::GenerateId();
  bitmap_map_[id] = memory.get();
  return std::make_unique<OwnedSharedBitmap>(std::move(memory), id);
}

std::unique_ptr<SharedBitmap> TestSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (bitmap_map_.find(id) == bitmap_map_.end())
    return nullptr;
  uint8_t* pixels = static_cast<uint8_t*>(bitmap_map_[id]->memory());
  return std::make_unique<UnownedSharedBitmap>(pixels, id);
}

bool TestSharedBitmapManager::ChildAllocatedSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const SharedBitmapId& id) {
  // TestSharedBitmapManager is both the client and service side. So the
  // notification here should be about a bitmap that was previously allocated
  // with AllocateSharedBitmap().
  DCHECK(bitmap_map_.find(id) != bitmap_map_.end());
  // The same bitmap id should not be notified more than once.
  DCHECK_EQ(notified_set_.count(id), 0u);
  notified_set_.insert(id);
  return true;
}

void TestSharedBitmapManager::ChildDeletedSharedBitmap(
    const SharedBitmapId& id) {
  // The bitmap id should previously have been given to
  // ChildAllocatedSharedBitmap().
  DCHECK_EQ(notified_set_.count(id), 1u);
  notified_set_.erase(id);
}

}  // namespace viz
