// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_shared_bitmap_manager.h"

#include "base/bind.h"

namespace cc {

void FreeSharedBitmap(SharedBitmap* shared_bitmap) {
  delete shared_bitmap->memory();
}

void IgnoreSharedBitmap(SharedBitmap* shared_bitmap) {}

TestSharedBitmapManager::TestSharedBitmapManager() {}

TestSharedBitmapManager::~TestSharedBitmapManager() {}

scoped_ptr<SharedBitmap> TestSharedBitmapManager::AllocateSharedBitmap(
    const gfx::Size& size) {
  base::AutoLock lock(lock_);
  scoped_ptr<base::SharedMemory> memory(new base::SharedMemory);
  memory->CreateAndMapAnonymous(size.GetArea() * 4);
  SharedBitmapId id = SharedBitmap::GenerateId();
  bitmap_map_[id] = memory.get();
  return scoped_ptr<SharedBitmap>(
      new SharedBitmap(memory.release(), id, base::Bind(&FreeSharedBitmap)));
}

scoped_ptr<SharedBitmap> TestSharedBitmapManager::GetSharedBitmapFromId(
    const gfx::Size&,
    const SharedBitmapId& id) {
  base::AutoLock lock(lock_);
  if (bitmap_map_.find(id) == bitmap_map_.end())
    return scoped_ptr<SharedBitmap>();
  return scoped_ptr<SharedBitmap>(
      new SharedBitmap(bitmap_map_[id], id, base::Bind(&IgnoreSharedBitmap)));
}

scoped_ptr<SharedBitmap> TestSharedBitmapManager::GetBitmapForSharedMemory(
    base::SharedMemory* memory) {
  base::AutoLock lock(lock_);
  SharedBitmapId id = SharedBitmap::GenerateId();
  bitmap_map_[id] = memory;
  return scoped_ptr<SharedBitmap>(
      new SharedBitmap(memory, id, base::Bind(&IgnoreSharedBitmap)));
}

}  // namespace cc
