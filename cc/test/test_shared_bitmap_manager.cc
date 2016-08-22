// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_shared_bitmap_manager.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"

namespace cc {

namespace {
class OwnedSharedBitmap : public SharedBitmap {
 public:
  OwnedSharedBitmap(std::unique_ptr<base::SharedMemory> shared_memory,
                    const SharedBitmapId& id)
      : SharedBitmap(static_cast<uint8_t*>(shared_memory->memory()), id),
        shared_memory_(std::move(shared_memory)) {}

  ~OwnedSharedBitmap() override {}

 private:
  std::unique_ptr<base::SharedMemory> shared_memory_;
};

}  // namespace

TestSharedBitmapManager::TestSharedBitmapManager() {}

TestSharedBitmapManager::~TestSharedBitmapManager() {}

std::unique_ptr<SharedBitmap> TestSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  std::unique_ptr<base::SharedMemory> memory(new base::SharedMemory);
  memory->CreateAndMapAnonymous(size.GetArea() * 4);
  SharedBitmapId id = SharedBitmap::GenerateId();
  bitmap_map_[id] = memory.get();
  return base::MakeUnique<OwnedSharedBitmap>(std::move(memory), id);
}

std::unique_ptr<SharedBitmap> TestSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (bitmap_map_.find(id) == bitmap_map_.end())
    return nullptr;
  uint8_t* pixels = static_cast<uint8_t*>(bitmap_map_[id]->memory());
  return base::MakeUnique<SharedBitmap>(pixels, id);
}

}  // namespace cc
