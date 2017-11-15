// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_shared_bitmap_manager.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"

namespace cc {

namespace {

static uint32_t g_next_sequence_number = 1;

class OwnedSharedBitmap : public viz::SharedBitmap {
 public:
  OwnedSharedBitmap(std::unique_ptr<base::SharedMemory> shared_memory,
                    const viz::SharedBitmapId& id)
      : viz::SharedBitmap(static_cast<uint8_t*>(shared_memory->memory()),
                          id,
                          g_next_sequence_number++),
        shared_memory_(std::move(shared_memory)) {}

  ~OwnedSharedBitmap() override {}

  // viz::SharedBitmap:
  base::SharedMemoryHandle GetSharedMemoryHandle() const override {
    return shared_memory_->handle();
  }

 private:
  std::unique_ptr<base::SharedMemory> shared_memory_;
};

class UnownedSharedBitmap : public viz::SharedBitmap {
 public:
  UnownedSharedBitmap(uint8_t* pixels, const viz::SharedBitmapId& id)
      : viz::SharedBitmap(pixels, id, g_next_sequence_number++) {}

  // viz::SharedBitmap:
  base::SharedMemoryHandle GetSharedMemoryHandle() const override {
    return base::SharedMemoryHandle();
  }
};

}  // namespace

TestSharedBitmapManager::TestSharedBitmapManager() {}

TestSharedBitmapManager::~TestSharedBitmapManager() {}

std::unique_ptr<viz::SharedBitmap>
TestSharedBitmapManager::AllocateSharedBitmap(const gfx::Size& size) {
  base::AutoLock lock(lock_);
  std::unique_ptr<base::SharedMemory> memory(new base::SharedMemory);
  memory->CreateAndMapAnonymous(size.GetArea() * 4);
  viz::SharedBitmapId id = viz::SharedBitmap::GenerateId();
  bitmap_map_[id] = memory.get();
  return std::make_unique<OwnedSharedBitmap>(std::move(memory), id);
}

std::unique_ptr<viz::SharedBitmap>
TestSharedBitmapManager::GetSharedBitmapFromId(const gfx::Size&,
                                               const viz::SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (bitmap_map_.find(id) == bitmap_map_.end())
    return nullptr;
  uint8_t* pixels = static_cast<uint8_t*>(bitmap_map_[id]->memory());
  return std::make_unique<UnownedSharedBitmap>(pixels, id);
}

}  // namespace cc
