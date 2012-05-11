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

  virtual bool client_has_memory_allocation_changed_callback() const {
    return true;
  }
  virtual bool has_surface_state() const {
    return surface_state_.surface_id != 0;
  }
  virtual const SurfaceState& surface_state() const {
    return surface_state_;
  }

  virtual gfx::Size GetSurfaceSize() const {
    return gfx::Size();
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

  virtual bool client_has_memory_allocation_changed_callback() const {
    return true;
  }
  virtual bool has_surface_state() const {
    return false;
  }
  virtual const SurfaceState& surface_state() const {
    NOTREACHED();
    static SurfaceState* surface_state_;
    return *surface_state_;
  }

  virtual gfx::Size GetSurfaceSize() const {
    return gfx::Size();
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

  static bool IsMoreImportant(GpuCommandBufferStubBase* lhs,
                              GpuCommandBufferStubBase* rhs) {
    return GpuMemoryManager::StubWithSurfaceComparator()(lhs, rhs);
  }

  static bool IsAllocationForegroundForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return alloc.suggest_have_frontbuffer &&
           alloc.suggest_have_backbuffer &&
           alloc.gpu_resource_size_in_bytes >=
               GpuMemoryManager::kMinimumAllocationForTab;
  }
  static bool IsAllocationBackgroundForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return alloc.suggest_have_frontbuffer &&
           !alloc.suggest_have_backbuffer &&
           alloc.gpu_resource_size_in_bytes == 0;
  }
  static bool IsAllocationHibernatedForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return !alloc.suggest_have_frontbuffer &&
           !alloc.suggest_have_backbuffer &&
           alloc.gpu_resource_size_in_bytes == 0;
  }
  static bool IsAllocationForegroundForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.suggest_have_frontbuffer &&
           !alloc.suggest_have_backbuffer &&
           alloc.gpu_resource_size_in_bytes ==
               GpuMemoryManager::kMinimumAllocationForTab;
  }
  static bool IsAllocationBackgroundForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.suggest_have_frontbuffer &&
           !alloc.suggest_have_backbuffer &&
           alloc.gpu_resource_size_in_bytes ==
               GpuMemoryManager::kMinimumAllocationForTab;
  }
  static bool IsAllocationHibernatedForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.suggest_have_frontbuffer &&
           !alloc.suggest_have_backbuffer &&
           alloc.gpu_resource_size_in_bytes == 0;
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
  EXPECT_FALSE(IsMoreImportant(&stub_true1, &stub_true1));
  EXPECT_FALSE(IsMoreImportant(&stub_true2, &stub_true2));
  EXPECT_FALSE(IsMoreImportant(&stub_true3, &stub_true3));
  EXPECT_FALSE(IsMoreImportant(&stub_false1, &stub_false1));
  EXPECT_FALSE(IsMoreImportant(&stub_false2, &stub_false2));
  EXPECT_FALSE(IsMoreImportant(&stub_false3, &stub_false3));

  // Visible should always be more important than non visible:
  EXPECT_TRUE(IsMoreImportant(&stub_true1, &stub_false1));
  EXPECT_TRUE(IsMoreImportant(&stub_true1, &stub_false2));
  EXPECT_TRUE(IsMoreImportant(&stub_true1, &stub_false3));
  EXPECT_TRUE(IsMoreImportant(&stub_true2, &stub_false1));
  EXPECT_TRUE(IsMoreImportant(&stub_true2, &stub_false2));
  EXPECT_TRUE(IsMoreImportant(&stub_true2, &stub_false3));
  EXPECT_TRUE(IsMoreImportant(&stub_true3, &stub_false1));
  EXPECT_TRUE(IsMoreImportant(&stub_true3, &stub_false2));
  EXPECT_TRUE(IsMoreImportant(&stub_true3, &stub_false3));

  // Not visible should never be more important than visible:
  EXPECT_FALSE(IsMoreImportant(&stub_false1, &stub_true1));
  EXPECT_FALSE(IsMoreImportant(&stub_false1, &stub_true2));
  EXPECT_FALSE(IsMoreImportant(&stub_false1, &stub_true3));
  EXPECT_FALSE(IsMoreImportant(&stub_false2, &stub_true1));
  EXPECT_FALSE(IsMoreImportant(&stub_false2, &stub_true2));
  EXPECT_FALSE(IsMoreImportant(&stub_false2, &stub_true3));
  EXPECT_FALSE(IsMoreImportant(&stub_false3, &stub_true1));
  EXPECT_FALSE(IsMoreImportant(&stub_false3, &stub_true2));
  EXPECT_FALSE(IsMoreImportant(&stub_false3, &stub_true3));

  // Newer should always be more important than older:
  EXPECT_TRUE(IsMoreImportant(&stub_true2, &stub_true1));
  EXPECT_TRUE(IsMoreImportant(&stub_true3, &stub_true1));
  EXPECT_TRUE(IsMoreImportant(&stub_true3, &stub_true2));
  EXPECT_TRUE(IsMoreImportant(&stub_false2, &stub_false1));
  EXPECT_TRUE(IsMoreImportant(&stub_false3, &stub_false1));
  EXPECT_TRUE(IsMoreImportant(&stub_false3, &stub_false2));

  // Older should never be more important than newer:
  EXPECT_FALSE(IsMoreImportant(&stub_true1, &stub_true2));
  EXPECT_FALSE(IsMoreImportant(&stub_true1, &stub_true3));
  EXPECT_FALSE(IsMoreImportant(&stub_true2, &stub_true3));
  EXPECT_FALSE(IsMoreImportant(&stub_false1, &stub_false2));
  EXPECT_FALSE(IsMoreImportant(&stub_false1, &stub_false3));
  EXPECT_FALSE(IsMoreImportant(&stub_false2, &stub_false3));
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
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));

  // Test stubs without surface, with share group of 1 stub.
  FakeCommandBufferStubWithoutSurface stub3, stub4;
  stub3.share_group_.push_back(&stub1);
  stub4.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);
  client_.stubs_.push_back(&stub4);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // Test stub without surface, with share group of multiple stubs.
  FakeCommandBufferStubWithoutSurface stub5;
  stub5.share_group_.push_back(&stub1);
  stub5.share_group_.push_back(&stub2);
  client_.stubs_.push_back(&stub5);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));
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
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));

  stub1.surface_state_.visible = false;
  stub2.surface_state_.visible = true;

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));
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
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub7.allocation_));
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
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));
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
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));

  stub3.surface_state_.last_used_time = older_;
  stub4.surface_state_.last_used_time = newer_;

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));
}

// Test GpuMemoryManager::Manage functionality: Test changing importance of
// enough stubs so that every stub in share group crosses threshold.
// Expect memory allocation of the stubs without surface to share memory
// allocation with the most visible stub in share group.
TEST_F(GpuMemoryManagerTest, TestManageChangingImportanceShareGroup) {
  FakeCommandBufferStub stubIgnoreA(GenerateUniqueSurfaceId(), true, newer_),
                        stubIgnoreB(GenerateUniqueSurfaceId(), false, newer_),
                        stubIgnoreC(GenerateUniqueSurfaceId(), false, newer_);
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, newest_),
                        stub2(GenerateUniqueSurfaceId(), true, newest_);
  client_.stubs_.push_back(&stubIgnoreA);
  client_.stubs_.push_back(&stubIgnoreB);
  client_.stubs_.push_back(&stubIgnoreC);
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
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  stub1.surface_state_.visible = false;

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  stub2.surface_state_.visible = false;

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  stub1.surface_state_.last_used_time = older_;

  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  stub2.surface_state_.last_used_time = older_;

  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub4.allocation_));
}

// Test GpuMemoryAllocation memory allocation bonuses:
// When the number of visible tabs is small, each tab should get a
// gpu_resource_size_in_bytes allocation value that is greater than
// kMinimumAllocationForTab, and when the number of tabs is large, each should
// get exactly kMinimumAllocationForTab and not less.
TEST_F(GpuMemoryManagerTest, TestForegroundStubsGetBonusAllocation) {
  FakeCommandBufferStub stub1(GenerateUniqueSurfaceId(), true, older_),
                        stub2(GenerateUniqueSurfaceId(), true, older_),
                        stub3(GenerateUniqueSurfaceId(), true, older_);
  client_.stubs_.push_back(&stub1);
  client_.stubs_.push_back(&stub2);
  client_.stubs_.push_back(&stub3);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub3.allocation_));
  EXPECT_GT(stub1.allocation_.gpu_resource_size_in_bytes,
            static_cast<size_t>(GpuMemoryManager::kMinimumAllocationForTab));
  EXPECT_GT(stub2.allocation_.gpu_resource_size_in_bytes,
            static_cast<size_t>(GpuMemoryManager::kMinimumAllocationForTab));
  EXPECT_GT(stub3.allocation_.gpu_resource_size_in_bytes,
            static_cast<size_t>(GpuMemoryManager::kMinimumAllocationForTab));

  FakeCommandBufferStub stub4(GenerateUniqueSurfaceId(), true, older_),
                        stub5(GenerateUniqueSurfaceId(), true, older_),
                        stub6(GenerateUniqueSurfaceId(), true, older_),
                        stub7(GenerateUniqueSurfaceId(), true, older_),
                        stub8(GenerateUniqueSurfaceId(), true, older_),
                        stub9(GenerateUniqueSurfaceId(), true, older_);
  client_.stubs_.push_back(&stub4);
  client_.stubs_.push_back(&stub5);
  client_.stubs_.push_back(&stub6);
  client_.stubs_.push_back(&stub7);
  client_.stubs_.push_back(&stub8);
  client_.stubs_.push_back(&stub9);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub3.allocation_));
  EXPECT_EQ(stub1.allocation_.gpu_resource_size_in_bytes,
            GpuMemoryManager::kMinimumAllocationForTab);
  EXPECT_EQ(stub2.allocation_.gpu_resource_size_in_bytes,
            GpuMemoryManager::kMinimumAllocationForTab);
  EXPECT_EQ(stub3.allocation_.gpu_resource_size_in_bytes,
            GpuMemoryManager::kMinimumAllocationForTab);
}

// Test GpuMemoryAllocation comparison operators: Iterate over all possible
// combinations of gpu_resource_size_in_bytes, suggest_have_backbuffer, and
// suggest_have_frontbuffer, and make sure allocations with equal values test
// equal and non equal values test not equal.
TEST_F(GpuMemoryManagerTest, GpuMemoryAllocationCompareTests) {
  std::vector<int> gpu_resource_size_in_bytes_values;
  gpu_resource_size_in_bytes_values.push_back(0);
  gpu_resource_size_in_bytes_values.push_back(1);
  gpu_resource_size_in_bytes_values.push_back(12345678);

  std::vector<int> suggested_buffer_allocation_values;
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasFrontbuffer |
      GpuMemoryAllocation::kHasBackbuffer);
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasFrontbuffer);
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasBackbuffer);
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasNoBuffers);

  for(size_t i = 0; i != gpu_resource_size_in_bytes_values.size(); ++i) {
    for(size_t j = 0; j != suggested_buffer_allocation_values.size(); ++j) {
      int sz = gpu_resource_size_in_bytes_values[i];
      int buffer_allocation = suggested_buffer_allocation_values[j];
      GpuMemoryAllocation allocation(sz, buffer_allocation);

      EXPECT_EQ(allocation, GpuMemoryAllocation(sz, buffer_allocation));
      EXPECT_NE(allocation, GpuMemoryAllocation(sz+1, buffer_allocation));

      for(size_t k = 0; k != suggested_buffer_allocation_values.size(); ++k) {
        int buffer_allocation_other = suggested_buffer_allocation_values[k];
        if (buffer_allocation == buffer_allocation_other) continue;
        EXPECT_NE(allocation, GpuMemoryAllocation(sz, buffer_allocation_other));
      }
    }
  }
}
