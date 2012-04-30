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

  virtual bool has_surface_state() const {
    return surface_state_.surface_id != 0;
  }
  virtual const SurfaceState& surface_state() const {
    return surface_state_;
  }

  virtual bool IsInSameContextShareGroup(
      const GpuCommandBufferStubBase& stub) const {
    return false;
  }
  virtual void SendMemoryAllocationToProxy(const GpuMemoryAllocation& alloc) {
  }
  virtual void SetMemoryAllocation(const GpuMemoryAllocation& alloc) {
    allocation_ = alloc;
  }
};

class FakeCommandBufferStubWithoutSurface : public GpuCommandBufferStubBase {
 public:
  GpuMemoryAllocation allocation_;
  std::vector<GpuCommandBufferStubBase*> share_group_;

  FakeCommandBufferStubWithoutSurface() {
  }

  virtual bool has_surface_state() const {
    return false;
  }
  virtual const SurfaceState& surface_state() const {
    NOTREACHED();
    static SurfaceState* surface_state_;
    return *surface_state_;
  }

  virtual bool IsInSameContextShareGroup(
      const GpuCommandBufferStubBase& stub) const {
    return std::find(share_group_.begin(),
                     share_group_.end(),
                     &stub) != share_group_.end();
  }
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
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer according
// to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageBasicFunctionality) {
  // Test stubs with surface.
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, older_),
                        stub2(GenerateUniqueSurfaceId(), false, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, false);

  // Test stubs without surface, with share group of 1 stub.
  FakeCommandBufferStubWithoutSurface stub3, stub4;
  stub3.share_group_.push_back(&stub1);
  stub4.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  Manage();
  EXPECT_EQ(stub1.allocation_, stub3.allocation_);
  EXPECT_EQ(stub2.allocation_, stub4.allocation_);

  // Test stub without surface, with share group of multiple stubs.
  FakeCommandBufferStubWithoutSurface stub5;
  stub5.share_group_.push_back(&stub1);
  stub5.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub5);

  Manage();
  EXPECT_EQ(stub1.allocation_, stub5.allocation_);
}

// Test GpuMemoryManager::Manage functionality: changing visibility.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer according
// to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageChangingVisibility) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, older_),
                        stub2(GenerateUniqueSurfaceId(), false, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);

  FakeCommandBufferStubWithoutSurface stub3, stub4;
  stub3.share_group_.push_back(&stub1);
  stub4.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  FakeCommandBufferStubWithoutSurface stub5;
  stub5.share_group_.push_back(&stub1);
  stub5.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub5);

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub1.allocation_, stub3.allocation_);
  EXPECT_EQ(stub2.allocation_, stub4.allocation_);
  EXPECT_EQ(stub1.allocation_, stub5.allocation_);

  stub1.surface_state_.visible = false;
  stub2.surface_state_.visible = true;

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub1.allocation_, stub3.allocation_);
  EXPECT_EQ(stub2.allocation_, stub4.allocation_);
  EXPECT_EQ(stub2.allocation_, stub5.allocation_);
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

  FakeCommandBufferStubWithoutSurface stub5, stub6;
  stub5.share_group_.push_back(&stub1);
  stub6.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub5);
  client_.stubs_.push_back(&stub6);

  FakeCommandBufferStubWithoutSurface stub7;
  stub7.share_group_.push_back(&stub1);
  stub7.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub7);

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub3.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub3.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub4.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub4.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub5.allocation_, stub1.allocation_);
  EXPECT_EQ(stub6.allocation_, stub2.allocation_);
  EXPECT_EQ(stub7.allocation_, stub1.allocation_);
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

  FakeCommandBufferStubWithoutSurface stub5, stub6;
  stub5.share_group_.push_back(&stub1);
  stub6.share_group_.push_back(&stub4);
  client_.stubs_.push_back(&stub5);
  client_.stubs_.push_back(&stub6);

  FakeCommandBufferStubWithoutSurface stub7;
  stub7.share_group_.push_back(&stub1);
  stub7.share_group_.push_back(&stub4);
  client_.stubs_.push_back(&stub7);

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub3.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub3.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub4.allocation_.suggest_have_frontbuffer, false);
  EXPECT_EQ(stub4.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub5.allocation_, stub1.allocation_);
  EXPECT_EQ(stub6.allocation_, stub4.allocation_);
  EXPECT_EQ(stub7.allocation_, stub1.allocation_);
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

  FakeCommandBufferStubWithoutSurface stub5, stub6;
  stub5.share_group_.push_back(&stub3);
  stub6.share_group_.push_back(&stub4);
  client_.stubs_.push_back(&stub5);
  client_.stubs_.push_back(&stub6);

  FakeCommandBufferStubWithoutSurface stub7;
  stub7.share_group_.push_back(&stub3);
  stub7.share_group_.push_back(&stub4);
  client_.stubs_.push_back(&stub7);

  Manage();
  EXPECT_EQ(stub3.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub3.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub4.allocation_.suggest_have_frontbuffer, false);
  EXPECT_EQ(stub4.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub5.allocation_, stub3.allocation_);
  EXPECT_EQ(stub6.allocation_, stub4.allocation_);
  EXPECT_EQ(stub7.allocation_, stub3.allocation_);

  stub3.surface_state_.last_used_time = older_;
  stub4.surface_state_.last_used_time = newer_;

  Manage();
  EXPECT_EQ(stub3.allocation_.suggest_have_frontbuffer, false);
  EXPECT_EQ(stub3.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub4.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub4.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub5.allocation_, stub3.allocation_);
  EXPECT_EQ(stub6.allocation_, stub4.allocation_);
  EXPECT_EQ(stub7.allocation_, stub4.allocation_);
}

// Test GpuMemoryManager::Manage functionality: Test changing importance of
// enough stubs so that every stub in share group crosses threshold.
// Expect memory allocation of the stubs without surface to share memory
// allocation with the most visible stub in share group.
TEST_F(GpuMemoryManagerTest, TestManageChangingImportanceShareGroup) {
  FakeCommandBufferStub stubA(GenerateUniqueSurfaceId(), true, newer_),
                        stubB(GenerateUniqueSurfaceId(), false, newer_),
                        stubC(GenerateUniqueSurfaceId(), false, newer_);
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, newest_),
                        stub2(GenerateUniqueSurfaceId(), true, newest_);
  client_.stubs_.push_back(&stubA);
  client_.stubs_.push_back(&stubB);
  client_.stubs_.push_back(&stubC);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);

  FakeCommandBufferStubWithoutSurface stub3, stub4;
  stub3.share_group_.push_back(&stub1);
  stub3.share_group_.push_back(&stub2);
  stub4.share_group_.push_back(&stub1);
  stub4.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, true);
  EXPECT_EQ(stub3.allocation_, stub1.allocation_);
  EXPECT_EQ(stub3.allocation_, stub2.allocation_);
  EXPECT_EQ(stub4.allocation_, stub1.allocation_);
  EXPECT_EQ(stub4.allocation_, stub2.allocation_);

  stub1.surface_state_.visible = false;

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, true);
  EXPECT_NE(stub3.allocation_, stub1.allocation_);
  EXPECT_EQ(stub3.allocation_, stub2.allocation_);
  EXPECT_NE(stub4.allocation_, stub1.allocation_);
  EXPECT_EQ(stub4.allocation_, stub2.allocation_);

  stub2.surface_state_.visible = false;

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub3.allocation_, stub1.allocation_);
  EXPECT_EQ(stub3.allocation_, stub2.allocation_);
  EXPECT_EQ(stub4.allocation_, stub1.allocation_);
  EXPECT_EQ(stub4.allocation_, stub2.allocation_);

  stub1.surface_state_.last_used_time = older_;

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, false);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, true);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, false);
  EXPECT_NE(stub3.allocation_, stub1.allocation_);
  EXPECT_EQ(stub3.allocation_, stub2.allocation_);
  EXPECT_NE(stub4.allocation_, stub1.allocation_);
  EXPECT_EQ(stub4.allocation_, stub2.allocation_);

  stub2.surface_state_.last_used_time = older_;

  Manage();
  EXPECT_EQ(stub1.allocation_.suggest_have_frontbuffer, false);
  EXPECT_EQ(stub1.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_frontbuffer, false);
  EXPECT_EQ(stub2.allocation_.suggest_have_backbuffer, false);
  EXPECT_EQ(stub3.allocation_, stub1.allocation_);
  EXPECT_EQ(stub3.allocation_, stub2.allocation_);
  EXPECT_EQ(stub4.allocation_, stub1.allocation_);
  EXPECT_EQ(stub4.allocation_, stub2.allocation_);
}

// Test GpuMemoryAllocation comparison operators: Iterate over all possible
// combinations of gpu_resource_size_in_bytes, suggest_have_backbuffer, and
// suggest_have_frontbuffer, and make sure allocations with equal values test
// equal and non equal values test not equal.
TEST_F(GpuMemoryManagerTest, GpuMemoryAllocationCompareTests) {
  int gpu_resource_size_in_bytes_values[] = { 0, 1, 12345678 };
  bool suggest_have_backbuffer_values[] = { false, true };
  bool suggest_have_frontbuffer_values[] = { false, true };

  for(int* sz = &gpu_resource_size_in_bytes_values[0];
      sz != &gpu_resource_size_in_bytes_values[3]; ++sz) {
    for(bool* shbb = &suggest_have_backbuffer_values[0];
        shbb != &suggest_have_backbuffer_values[2]; ++shbb) {
      for(bool* shfb = &suggest_have_frontbuffer_values[0];
          shfb != &suggest_have_frontbuffer_values[2]; ++shfb) {
        GpuMemoryAllocation allocation(*sz, *shbb, *shfb);
        EXPECT_EQ(allocation, GpuMemoryAllocation(*sz, *shbb, *shfb));
        EXPECT_NE(allocation, GpuMemoryAllocation(*sz+1, *shbb, *shfb));
        EXPECT_NE(allocation, GpuMemoryAllocation(*sz, !*shbb, *shfb));
        EXPECT_NE(allocation, GpuMemoryAllocation(*sz, *shbb, !*shfb));
      }
    }
  }
}
