// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

class FakeCommandBufferStub : public GpuCommandBufferStubBase {
 public:
  SurfaceState surface_state_;
  GpuMemoryAllocation allocation_;

  FakeCommandBufferStub()
      : surface_state_(0, false, base::TimeTicks()) {
  }

  FakeCommandBufferStub(int32 surface_id,
                        bool visible,
                        base::TimeTicks last_used_time)
      : surface_state_(surface_id, visible, last_used_time) {
  }

  virtual bool has_surface_state() { return surface_state_.surface_id != 0; }
  virtual const SurfaceState& surface_state() { return surface_state_; }
  virtual void SendMemoryAllocationToProxy(const GpuMemoryAllocation& alloc) {
  }
  virtual void SetMemoryAllocation(const GpuMemoryAllocation& alloc) {
    allocation_ = alloc;
  }
};

class FakeClient : public GpuMemoryManagerClient {
 public:
  std::vector<GpuCommandBufferStubBase*> stubs_;

  virtual void AppendAllCommandBufferStubs(
      std::vector<GpuCommandBufferStubBase*>& stubs) {
    stubs.insert(stubs.end(), stubs_.begin(), stubs_.end());
  }
};

class GpuMemoryManagerTest : public testing::Test {
 protected:
  static const size_t kFrontbufferLimitForTest = 3;

  GpuMemoryManagerTest()
      : memory_manager_(&client_, kFrontbufferLimitForTest) {
  }

  virtual void SetUp() {
    older_ = base::TimeTicks::FromInternalValue(1);
    newer_ = base::TimeTicks::FromInternalValue(2);
    newest_ = base::TimeTicks::FromInternalValue(3);
  }

  static int32 GenerateUniqueSurfaceId() {
    static int32 surface_id_ = 1;
    return surface_id_++;
  }

  static bool is_more_important(GpuCommandBufferStubBase* lhs,
                                GpuCommandBufferStubBase* rhs) {
    return GpuMemoryManager::StubWithSurfaceComparator()(lhs, rhs);
  }

  void Manage() {
    memory_manager_.Manage();
  }

  base::TimeTicks older_, newer_, newest_;
  FakeClient client_;
  GpuMemoryManager memory_manager_;
};


// Create fake stubs with every combination of {visibilty,last_use_time}
// and make sure they compare correctly.  Only compare stubs with surfaces.
// Expect {more visible, newer} surfaces to be more important, in that order.
TEST_F(GpuMemoryManagerTest, ComparatorTests) {
  FakeCommandBufferStub stub_true1(GenerateUniqueSurfaceId(), true, older_),
                        stub_true2(GenerateUniqueSurfaceId(), true, newer_),
                        stub_true3(GenerateUniqueSurfaceId(), true, newest_),
                        stub_false1(GenerateUniqueSurfaceId(), false, older_),
                        stub_false2(GenerateUniqueSurfaceId(), false, newer_),
                        stub_false3(GenerateUniqueSurfaceId(), false, newest_);

  // Should never be more important than self:
  EXPECT_FALSE(is_more_important(&stub_true1, &stub_true1));
  EXPECT_FALSE(is_more_important(&stub_true2, &stub_true2));
  EXPECT_FALSE(is_more_important(&stub_true3, &stub_true3));
  EXPECT_FALSE(is_more_important(&stub_false1, &stub_false1));
  EXPECT_FALSE(is_more_important(&stub_false2, &stub_false2));
  EXPECT_FALSE(is_more_important(&stub_false3, &stub_false3));

  // Visible should always be more important than non visible:
  EXPECT_TRUE(is_more_important(&stub_true1, &stub_false1));
  EXPECT_TRUE(is_more_important(&stub_true1, &stub_false2));
  EXPECT_TRUE(is_more_important(&stub_true1, &stub_false3));
  EXPECT_TRUE(is_more_important(&stub_true2, &stub_false1));
  EXPECT_TRUE(is_more_important(&stub_true2, &stub_false2));
  EXPECT_TRUE(is_more_important(&stub_true2, &stub_false3));
  EXPECT_TRUE(is_more_important(&stub_true3, &stub_false1));
  EXPECT_TRUE(is_more_important(&stub_true3, &stub_false2));
  EXPECT_TRUE(is_more_important(&stub_true3, &stub_false3));

  // Not visible should never be more important than visible:
  EXPECT_FALSE(is_more_important(&stub_false1, &stub_true1));
  EXPECT_FALSE(is_more_important(&stub_false1, &stub_true2));
  EXPECT_FALSE(is_more_important(&stub_false1, &stub_true3));
  EXPECT_FALSE(is_more_important(&stub_false2, &stub_true1));
  EXPECT_FALSE(is_more_important(&stub_false2, &stub_true2));
  EXPECT_FALSE(is_more_important(&stub_false2, &stub_true3));
  EXPECT_FALSE(is_more_important(&stub_false3, &stub_true1));
  EXPECT_FALSE(is_more_important(&stub_false3, &stub_true2));
  EXPECT_FALSE(is_more_important(&stub_false3, &stub_true3));

  // Newer should always be more important than older:
  EXPECT_TRUE(is_more_important(&stub_true2, &stub_true1));
  EXPECT_TRUE(is_more_important(&stub_true3, &stub_true1));
  EXPECT_TRUE(is_more_important(&stub_true3, &stub_true2));
  EXPECT_TRUE(is_more_important(&stub_false2, &stub_false1));
  EXPECT_TRUE(is_more_important(&stub_false3, &stub_false1));
  EXPECT_TRUE(is_more_important(&stub_false3, &stub_false2));

  // Older should never be more important than newer:
  EXPECT_FALSE(is_more_important(&stub_true1, &stub_true2));
  EXPECT_FALSE(is_more_important(&stub_true1, &stub_true3));
  EXPECT_FALSE(is_more_important(&stub_true2, &stub_true3));
  EXPECT_FALSE(is_more_important(&stub_false1, &stub_false2));
  EXPECT_FALSE(is_more_important(&stub_false1, &stub_false3));
  EXPECT_FALSE(is_more_important(&stub_false2, &stub_false3));
}

// Test GpuMemoryManager::Manage basic functionality.
// Expect memory allocation to set has_frontbuffer, has_backbuffer according
// to visibility and last used time.
TEST_F(GpuMemoryManagerTest, TestManageBasicFunctionality) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, older_),
                        stub2(GenerateUniqueSurfaceId(), false, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);

  Manage();
  EXPECT_EQ(stub1.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.has_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_backbuffer, false);
}

// Test GpuMemoryManager::Manage functionality: Test changing visibility
// Expect memory allocation to set has_frontbuffer, has_backbuffer according
// to visibility and last used time.
TEST_F(GpuMemoryManagerTest, TestManageChangingVisibility) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, older_),
                        stub2(GenerateUniqueSurfaceId(), false, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);

  Manage();
  EXPECT_EQ(stub1.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.has_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_backbuffer, false);

  stub1.surface_state_.visible = false;
  stub2.surface_state_.visible = true;

  Manage();
  EXPECT_EQ(stub1.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.has_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_backbuffer, true);
}

// Test GpuMemoryManager::Manage functionality: Test more than threshold number
// of visible stubs.
// Expect all allocations to continue to have frontbuffer.
TEST_F(GpuMemoryManagerTest, TestManageManyVisibleStubs) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, older_),
                        stub2(GenerateUniqueSurfaceId(), true, older_),
                        stub3(GenerateUniqueSurfaceId(), true, older_),
                        stub4(GenerateUniqueSurfaceId(), true, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  Manage();
  EXPECT_EQ(stub1.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.has_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_backbuffer, true);
  EXPECT_EQ(stub3.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub3.allocation_.has_backbuffer, true);
  EXPECT_EQ(stub4.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub4.allocation_.has_backbuffer, true);
}

// Test GpuMemoryManager::Manage functionality: Test more than threshold number
// of not visible stubs.
// Expect the stubs surpassing the threshold to not have a backbuffer.
TEST_F(GpuMemoryManagerTest, TestManageManyNotVisibleStubs) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), false, newer_),
                        stub2(GenerateUniqueSurfaceId(), false, newer_),
                        stub3(GenerateUniqueSurfaceId(), false, newer_),
                        stub4(GenerateUniqueSurfaceId(), false, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  Manage();
  EXPECT_EQ(stub1.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.has_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.has_backbuffer, false);
  EXPECT_EQ(stub3.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub3.allocation_.has_backbuffer, false);
  EXPECT_EQ(stub4.allocation_.has_frontbuffer, false);
  EXPECT_EQ(stub4.allocation_.has_backbuffer, false);
}

// Test GpuMemoryManager::Manage functionality: Test changing the last used
// time of stubs when doing so causes change in which stubs surpass threshold.
// Expect frontbuffer to be dropped for the older stub.
TEST_F(GpuMemoryManagerTest, TestManageChangingLastUsedTime) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), false, newer_),
                        stub2(GenerateUniqueSurfaceId(), false, newer_),
                        stub3(GenerateUniqueSurfaceId(), false, newer_),
                        stub4(GenerateUniqueSurfaceId(), false, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  Manage();
  EXPECT_EQ(stub3.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub3.allocation_.has_backbuffer, false);
  EXPECT_EQ(stub4.allocation_.has_frontbuffer, false);
  EXPECT_EQ(stub4.allocation_.has_backbuffer, false);

  stub3.surface_state_.last_used_time = older_;
  stub4.surface_state_.last_used_time = newer_;

  Manage();
  EXPECT_EQ(stub3.allocation_.has_frontbuffer, false);
  EXPECT_EQ(stub3.allocation_.has_backbuffer, false);
  EXPECT_EQ(stub4.allocation_.has_frontbuffer, true);
  EXPECT_EQ(stub4.allocation_.has_backbuffer, false);
}
