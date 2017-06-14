// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string.h>

#include "components/viz/service/display_compositor/host_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class HostSharedBitmapManagerTest : public testing::Test {
 protected:
  void SetUp() override { manager_.reset(new HostSharedBitmapManager()); }
  std::unique_ptr<HostSharedBitmapManager> manager_;
};

TEST_F(HostSharedBitmapManagerTest, TestCreate) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  std::unique_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();

  HostSharedBitmapManagerClient client(manager_.get());
  base::SharedMemoryHandle handle = bitmap->handle().Duplicate();
  client.ChildAllocatedSharedBitmap(size_in_bytes, handle, id);

  std::unique_ptr<cc::SharedBitmap> large_bitmap;
  large_bitmap = manager_->GetSharedBitmapFromId(gfx::Size(1024, 1024), id);
  EXPECT_TRUE(large_bitmap.get() == NULL);

  std::unique_ptr<cc::SharedBitmap> very_large_bitmap;
  very_large_bitmap =
      manager_->GetSharedBitmapFromId(gfx::Size(1, (1 << 30) | 1), id);
  EXPECT_TRUE(very_large_bitmap.get() == NULL);

  std::unique_ptr<cc::SharedBitmap> negative_size_bitmap;
  negative_size_bitmap =
      manager_->GetSharedBitmapFromId(gfx::Size(-1, 1024), id);
  EXPECT_TRUE(negative_size_bitmap.get() == NULL);

  cc::SharedBitmapId id2 = cc::SharedBitmap::GenerateId();
  std::unique_ptr<cc::SharedBitmap> invalid_bitmap;
  invalid_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id2);
  EXPECT_TRUE(invalid_bitmap.get() == NULL);

  std::unique_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  ASSERT_TRUE(shared_bitmap.get() != NULL);
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), 4), 0);

  std::unique_ptr<cc::SharedBitmap> large_bitmap2;
  large_bitmap2 = manager_->GetSharedBitmapFromId(gfx::Size(1024, 1024), id);
  EXPECT_TRUE(large_bitmap2.get() == NULL);

  std::unique_ptr<cc::SharedBitmap> shared_bitmap2;
  shared_bitmap2 = manager_->GetSharedBitmapFromId(bitmap_size, id);
  EXPECT_TRUE(shared_bitmap2->pixels() == shared_bitmap->pixels());
  shared_bitmap2.reset();
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);

  client.DidDeleteSharedBitmap(id);

  memset(bitmap->memory(), 0, size_in_bytes);

  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);
  bitmap.reset();
  shared_bitmap.reset();
}

TEST_F(HostSharedBitmapManagerTest, RemoveProcess) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  std::unique_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();

  std::unique_ptr<HostSharedBitmapManagerClient> client(
      new HostSharedBitmapManagerClient(manager_.get()));
  base::SharedMemoryHandle handle = bitmap->handle().Duplicate();
  client->ChildAllocatedSharedBitmap(size_in_bytes, handle, id);

  std::unique_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  ASSERT_TRUE(shared_bitmap.get() != NULL);

  EXPECT_EQ(1u, manager_->AllocatedBitmapCount());
  client.reset();
  EXPECT_EQ(0u, manager_->AllocatedBitmapCount());

  std::unique_ptr<cc::SharedBitmap> shared_bitmap2;
  shared_bitmap2 = manager_->GetSharedBitmapFromId(bitmap_size, id);
  EXPECT_TRUE(shared_bitmap2.get() == NULL);
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);

  shared_bitmap.reset();
}

TEST_F(HostSharedBitmapManagerTest, AddDuplicate) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  std::unique_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();
  HostSharedBitmapManagerClient client(manager_.get());

  base::SharedMemoryHandle handle = bitmap->handle().Duplicate();
  client.ChildAllocatedSharedBitmap(size_in_bytes, handle, id);

  std::unique_ptr<base::SharedMemory> bitmap2(new base::SharedMemory());
  bitmap2->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap2->memory(), 0x00, size_in_bytes);

  client.ChildAllocatedSharedBitmap(size_in_bytes, bitmap2->handle(), id);

  std::unique_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(bitmap_size, id);
  ASSERT_TRUE(shared_bitmap.get() != NULL);
  EXPECT_EQ(memcmp(shared_bitmap->pixels(), bitmap->memory(), size_in_bytes),
            0);
  client.DidDeleteSharedBitmap(id);
}

TEST_F(HostSharedBitmapManagerTest, SharedMemoryHandle) {
  gfx::Size bitmap_size(1, 1);
  size_t size_in_bytes;
  EXPECT_TRUE(cc::SharedBitmap::SizeInBytes(bitmap_size, &size_in_bytes));
  std::unique_ptr<base::SharedMemory> bitmap(new base::SharedMemory());
  auto shared_memory_guid = bitmap->handle().GetGUID();
  bitmap->CreateAndMapAnonymous(size_in_bytes);
  memset(bitmap->memory(), 0xff, size_in_bytes);
  cc::SharedBitmapId id = cc::SharedBitmap::GenerateId();

  HostSharedBitmapManagerClient client(manager_.get());
  base::SharedMemoryHandle handle = bitmap->handle().Duplicate();
  client.ChildAllocatedSharedBitmap(size_in_bytes, handle, id);

  std::unique_ptr<cc::SharedBitmap> shared_bitmap;
  shared_bitmap = manager_->GetSharedBitmapFromId(gfx::Size(1, 1), id);
  EXPECT_EQ(shared_bitmap->GetSharedMemoryHandle().GetGUID(),
            shared_memory_guid);

  client.DidDeleteSharedBitmap(id);
}

}  // namespace
}  // namespace viz
