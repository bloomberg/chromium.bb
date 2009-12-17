// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has the unit tests for the IdAllocator class.

#include "gpu/command_buffer/client/id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using gpu::ResourceId;

class IdAllocatorTest : public testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  IdAllocator* id_allocator() { return &id_allocator_; }

 private:
  IdAllocator id_allocator_;
};

// Checks basic functionality: AllocateID, FreeID, InUse.
TEST_F(IdAllocatorTest, TestBasic) {
  IdAllocator *allocator = id_allocator();
  // Check that resource 0 is not in use
  EXPECT_FALSE(allocator->InUse(0));

  // Allocate an ID, check that it's in use.
  ResourceId id1 = allocator->AllocateID();
  EXPECT_TRUE(allocator->InUse(id1));

  // Allocate another ID, check that it's in use, and different from the first
  // one.
  ResourceId id2 = allocator->AllocateID();
  EXPECT_TRUE(allocator->InUse(id2));
  EXPECT_NE(id1, id2);

  // Free one of the IDs, check that it's not in use any more.
  allocator->FreeID(id1);
  EXPECT_FALSE(allocator->InUse(id1));

  // Frees the other ID, check that it's not in use any more.
  allocator->FreeID(id2);
  EXPECT_FALSE(allocator->InUse(id2));
}

// Checks that the resource IDs are allocated conservatively, and re-used after
// being freed.
TEST_F(IdAllocatorTest, TestAdvanced) {
  IdAllocator *allocator = id_allocator();

  // Allocate a significant number of resources.
  const unsigned int kNumResources = 100;
  ResourceId ids[kNumResources];
  for (unsigned int i = 0; i < kNumResources; ++i) {
    ids[i] = allocator->AllocateID();
    EXPECT_TRUE(allocator->InUse(ids[i]));
  }

  // Check that the allocation is conservative with resource IDs, that is that
  // the resource IDs don't go over kNumResources - so that the service doesn't
  // have to allocate too many internal structures when the resources are used.
  for (unsigned int i = 0; i < kNumResources; ++i) {
    EXPECT_GT(kNumResources, ids[i]);
  }

  // Check that the next resources are still free.
  for (unsigned int i = 0; i < kNumResources; ++i) {
    EXPECT_FALSE(allocator->InUse(kNumResources + i));
  }

  // Check that a new allocation re-uses the resource we just freed.
  ResourceId id1 = ids[kNumResources / 2];
  allocator->FreeID(id1);
  EXPECT_FALSE(allocator->InUse(id1));
  ResourceId id2 = allocator->AllocateID();
  EXPECT_TRUE(allocator->InUse(id2));
  EXPECT_EQ(id1, id2);
}

}  // namespace gpu
