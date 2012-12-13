// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "ui/gfx/size_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

class FakeMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  void TrackMemoryAllocatedChange(
      size_t old_size,
      size_t new_size,
      gpu::gles2::MemoryTracker::Pool pool) {
  }
 private:
  ~FakeMemoryTracker() {
  }
};

namespace content {

// This class is used to collect all stub assignments during a
// Manage() call.
class ClientAssignmentCollector {
 public:
  struct ClientMemoryStat {
    GpuMemoryAllocation allocation;
  };
  typedef base::hash_map<GpuMemoryManagerClient*, ClientMemoryStat>
      ClientMemoryStatMap;

  static const ClientMemoryStatMap& GetClientStatsForLastManage() {
    return client_memory_stats_for_last_manage_;
  }
  static void ClearAllStats() {
    client_memory_stats_for_last_manage_.clear();
  }
  static void AddClientStat(GpuMemoryManagerClient* client,
                          const GpuMemoryAllocation& allocation) {
    DCHECK(!client_memory_stats_for_last_manage_.count(client));
    client_memory_stats_for_last_manage_[client].allocation = allocation;
  }

 private:
  static ClientMemoryStatMap client_memory_stats_for_last_manage_;
};

ClientAssignmentCollector::ClientMemoryStatMap
    ClientAssignmentCollector::client_memory_stats_for_last_manage_;

class FakeClient : public GpuMemoryManagerClient {
 public:
  GpuMemoryManager& memmgr_;
  GpuMemoryAllocation allocation_;
  size_t total_gpu_memory_;
  scoped_refptr<gpu::gles2::MemoryTracker> memory_tracker_;
  gpu::gles2::MemoryTracker* overridden_memory_tracker_;
  gfx::Size surface_size_;

  // This will create a client with no surface
  FakeClient(GpuMemoryManager& memmgr)
      : memmgr_(memmgr)
      , total_gpu_memory_(0)
      , memory_tracker_(new FakeMemoryTracker())
      , overridden_memory_tracker_(0) {
    memmgr_.AddClient(this, false, true, base::TimeTicks());
  }

  // This will create a client with a surface
  FakeClient(GpuMemoryManager& memmgr,
             int32 surface_id,
             bool visible,
             base::TimeTicks last_used_time)
      : memmgr_(memmgr)
      , total_gpu_memory_(0)
      , memory_tracker_(new FakeMemoryTracker())
      , overridden_memory_tracker_(0) {
    memmgr_.AddClient(this, surface_id != 0, visible, last_used_time);
  }

  ~FakeClient() {
    memmgr_.RemoveClient(this);
  }

  void SetMemoryAllocation(const GpuMemoryAllocation& alloc) {
    allocation_ = alloc;
    ClientAssignmentCollector::AddClientStat(this, alloc);
  }

  bool GetTotalGpuMemory(size_t* bytes) {
    if (total_gpu_memory_) {
      *bytes = total_gpu_memory_;
      return true;
    }
    return false;
  }
  void SetTotalGpuMemory(size_t bytes) { total_gpu_memory_ = bytes; }

  gpu::gles2::MemoryTracker* GetMemoryTracker() const OVERRIDE {
    if (overridden_memory_tracker_)
      return overridden_memory_tracker_;
    return memory_tracker_.get();
  }

  void SetInSameShareGroup(GpuMemoryManagerClient* stub) {
    overridden_memory_tracker_ = stub->GetMemoryTracker();
  }

  gfx::Size GetSurfaceSize() const {
    return surface_size_;
  }
  void SetSurfaceSize(gfx::Size size) { surface_size_ = size; }

  void SetVisible(bool visible) {
    memmgr_.SetClientVisible(this, visible);
  }

  void SetUsageStats(size_t bytes_required,
                     size_t bytes_nice_to_have,
                     size_t bytes_allocated) {
    memmgr_.SetClientManagedMemoryStats(
        this,
        GpuManagedMemoryStats(bytes_required,
                              bytes_nice_to_have,
                              bytes_allocated,
                              false));
  }

  size_t BytesWhenNotVisible() const {
    return allocation_.renderer_allocation.bytes_limit_when_not_visible;
  }
};

class GpuMemoryManagerTest : public testing::Test {
 protected:
  static const size_t kFrontbufferLimitForTest = 3;

  GpuMemoryManagerTest()
      : memmgr_(0, kFrontbufferLimitForTest) {
    memmgr_.TestingDisableScheduleManage();
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

  bool IsAllocationForegroundForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_visible >=
               GetMinimumTabAllocation();
  }
  bool IsAllocationBackgroundForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_not_visible <=
               memmgr_.GetCurrentBackgroundedAvailableGpuMemory();
  }
  bool IsAllocationHibernatedForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_not_visible <=
               memmgr_.GetCurrentBackgroundedAvailableGpuMemory();
  }
  bool IsAllocationForegroundForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_visible ==
               GetMinimumTabAllocation();
  }
  bool IsAllocationBackgroundForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_visible ==
               GetMinimumTabAllocation();
  }
  bool IsAllocationHibernatedForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_visible == 0;
  }

  void Manage() {
    ClientAssignmentCollector::ClearAllStats();
    memmgr_.Manage();
  }

  size_t CalcAvailableFromGpuTotal(size_t bytes) {
    return GpuMemoryManager::CalcAvailableFromGpuTotal(bytes);
  }

  size_t CalcAvailableFromViewportArea(int viewport_area) {
    return GpuMemoryManager::CalcAvailableFromViewportArea(viewport_area);
  }

  size_t CalcAvailableClamped(size_t bytes) {
    bytes = std::max(bytes, memmgr_.GetDefaultAvailableGpuMemory());
    bytes = std::min(bytes, memmgr_.GetMaximumTotalGpuMemory());
    return bytes;
  }

  size_t GetAvailableGpuMemory() {
    return memmgr_.GetAvailableGpuMemory();
  }

  size_t GetMaximumTabAllocation() {
    return memmgr_.GetMaximumTabAllocation();
  }

  size_t GetMinimumTabAllocation() {
    return memmgr_.GetMinimumTabAllocation();
  }

  base::TimeTicks older_, newer_, newest_;
  GpuMemoryManager memmgr_;
};

// Create fake stubs with every combination of {visibilty,last_use_time}
// and make sure they compare correctly.  Only compare stubs with surfaces.
// Expect {more visible, newer} surfaces to be more important, in that order.
TEST_F(GpuMemoryManagerTest, ComparatorTests) {
  FakeClient
      stub_true1(memmgr_, GenerateUniqueSurfaceId(), true, older_),
      stub_true2(memmgr_, GenerateUniqueSurfaceId(), true, newer_),
      stub_true3(memmgr_, GenerateUniqueSurfaceId(), true, newest_),
      stub_false1(memmgr_, GenerateUniqueSurfaceId(), false, older_),
      stub_false2(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
      stub_false3(memmgr_, GenerateUniqueSurfaceId(), false, newest_);

  // Should never be more important than self:
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_true1, &stub_true1));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_true2, &stub_true2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_true3, &stub_true3));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false1, &stub_false1));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false2, &stub_false2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false3, &stub_false3));

  // Visible should always be more important than non visible:
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true1, &stub_false1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true1, &stub_false2));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true1, &stub_false3));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true2, &stub_false1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true2, &stub_false2));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true2, &stub_false3));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true3, &stub_false1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true3, &stub_false2));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true3, &stub_false3));

  // Not visible should never be more important than visible:
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false1, &stub_true1));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false1, &stub_true2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false1, &stub_true3));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false2, &stub_true1));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false2, &stub_true2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false2, &stub_true3));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false3, &stub_true1));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false3, &stub_true2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false3, &stub_true3));

  // Newer should always be more important than older:
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true2, &stub_true1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true3, &stub_true1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_true3, &stub_true2));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_false2, &stub_false1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_false3, &stub_false1));
  EXPECT_TRUE(memmgr_.TestingCompareClients(&stub_false3, &stub_false2));

  // Older should never be more important than newer:
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_true1, &stub_true2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_true1, &stub_true3));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_true2, &stub_true3));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false1, &stub_false2));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false1, &stub_false3));
  EXPECT_FALSE(memmgr_.TestingCompareClients(&stub_false2, &stub_false3));
}

// Test GpuMemoryManager::Manage basic functionality.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer
// according to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageBasicFunctionality) {
  // Test stubs with surface.
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), false, older_);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));

  // Test stubs without surface, with share group of 1 stub.
  FakeClient stub3(memmgr_), stub4(memmgr_);
  stub3.SetInSameShareGroup(&stub1);
  stub4.SetInSameShareGroup(&stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // Test stub without surface, with share group of multiple stubs.
  FakeClient stub5(memmgr_);
  stub5.SetInSameShareGroup(&stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));
}

// Test GpuMemoryManager::Manage functionality: changing visibility.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer
// according to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageChangingVisibility) {
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), false, older_);

  FakeClient stub3(memmgr_), stub4(memmgr_);
  stub3.SetInSameShareGroup(&stub1);
  stub4.SetInSameShareGroup(&stub2);

  FakeClient stub5(memmgr_);
  stub5.SetInSameShareGroup(&stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));

  memmgr_.TestingSetClientVisible(&stub1, false);
  memmgr_.TestingSetClientVisible(&stub2, true);

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
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub3(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub4(memmgr_, GenerateUniqueSurfaceId(), true, older_);

  FakeClient stub5(memmgr_), stub6(memmgr_);
  stub5.SetInSameShareGroup(&stub1);
  stub6.SetInSameShareGroup(&stub2);

  FakeClient stub7(memmgr_);
  stub7.SetInSameShareGroup(&stub2);

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
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stub3(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stub4(memmgr_, GenerateUniqueSurfaceId(), false, older_);

  FakeClient stub5(memmgr_), stub6(memmgr_);
  stub5.SetInSameShareGroup(&stub1);
  stub6.SetInSameShareGroup(&stub4);

  FakeClient stub7(memmgr_);
  stub7.SetInSameShareGroup(&stub1);

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
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stub3(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stub4(memmgr_, GenerateUniqueSurfaceId(), false, older_);

  FakeClient stub5(memmgr_), stub6(memmgr_);
  stub5.SetInSameShareGroup(&stub3);
  stub6.SetInSameShareGroup(&stub4);

  FakeClient stub7(memmgr_);
  stub7.SetInSameShareGroup(&stub3);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));

  memmgr_.TestingSetClientLastUsedTime(&stub3, older_);
  memmgr_.TestingSetClientLastUsedTime(&stub4, newer_);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub7.allocation_));
}

// Test GpuMemoryManager::Manage functionality: Test changing importance of
// enough stubs so that every stub in share group crosses threshold.
// Expect memory allocation of the stubs without surface to share memory
// allocation with the most visible stub in share group.
TEST_F(GpuMemoryManagerTest, TestManageChangingImportanceShareGroup) {
  FakeClient stubIgnoreA(memmgr_, GenerateUniqueSurfaceId(), true, newer_),
             stubIgnoreB(memmgr_, GenerateUniqueSurfaceId(), false, newer_),
             stubIgnoreC(memmgr_, GenerateUniqueSurfaceId(), false, newer_);
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, newest_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), true, newest_);

  FakeClient stub3(memmgr_), stub4(memmgr_);
  stub3.SetInSameShareGroup(&stub2);
  stub4.SetInSameShareGroup(&stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  memmgr_.TestingSetClientVisible(&stub1, false);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  memmgr_.TestingSetClientVisible(&stub2, false);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  memmgr_.TestingSetClientLastUsedTime(&stub1, older_);

  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  memmgr_.TestingSetClientLastUsedTime(&stub2, older_);

  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub4.allocation_));
}

// Test GpuMemoryAllocation memory allocation bonuses:
// When the number of visible tabs is small, each tab should get a
// gpu_resource_size_in_bytes allocation value that is greater than
// GetMinimumTabAllocation(), and when the number of tabs is large, each should
// get exactly GetMinimumTabAllocation() and not less.
TEST_F(GpuMemoryManagerTest, TestForegroundStubsGetBonusAllocation) {
  size_t max_stubs_before_no_bonus =
      GetAvailableGpuMemory() / (GetMinimumTabAllocation() + 1);

  std::vector<FakeClient*> stubs;
  for (size_t i = 0; i < max_stubs_before_no_bonus; ++i) {
    stubs.push_back(
        new FakeClient(memmgr_, GenerateUniqueSurfaceId(), true, older_));
  }

  Manage();
  for (size_t i = 0; i < stubs.size(); ++i) {
    EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stubs[i]->allocation_));
    EXPECT_GT(
        stubs[i]->allocation_.renderer_allocation.bytes_limit_when_visible,
        static_cast<size_t>(GetMinimumTabAllocation()));
  }

  FakeClient extra_stub(memmgr_, GenerateUniqueSurfaceId(), true, older_);

  Manage();
  for (size_t i = 0; i < stubs.size(); ++i) {
    EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stubs[i]->allocation_));
    EXPECT_EQ(
        stubs[i]->allocation_.renderer_allocation.bytes_limit_when_visible,
        GetMinimumTabAllocation());
  }

  for (size_t i = 0; i < max_stubs_before_no_bonus; ++i) {
    delete stubs[i];
  }
}

// Test GpuMemoryManager::UpdateAvailableGpuMemory functionality
TEST_F(GpuMemoryManagerTest, TestUpdateAvailableGpuMemory) {
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), false, older_),
             stub3(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub4(memmgr_, GenerateUniqueSurfaceId(), false, older_);

#if defined(OS_ANDROID)
  // We use the largest visible surface size to calculate the limit
  stub1.SetSurfaceSize(gfx::Size(1024, 512)); // Surface size
  stub2.SetSurfaceSize(gfx::Size(2048, 512)); // Larger but not visible.
  stub3.SetSurfaceSize(gfx::Size(512, 512));  // Visible but smaller.
  stub4.SetSurfaceSize(gfx::Size(512, 512));  // Not visible and smaller.
  Manage();
  size_t bytes_expected = CalcAvailableFromViewportArea(1024*512);
#else
  // We take the lowest GPU's total memory as the limit
  size_t expected = 400 * 1024 * 1024;
  stub1.SetTotalGpuMemory(expected); // GPU Memory
  stub2.SetTotalGpuMemory(expected - 1024 * 1024); // Smaller but not visible.
  stub3.SetTotalGpuMemory(expected + 1024 * 1024); // Visible but larger.
  stub4.SetTotalGpuMemory(expected + 1024 * 1024); // Not visible and larger.
  Manage();
  size_t bytes_expected = CalcAvailableFromGpuTotal(expected);
#endif
  EXPECT_EQ(GetAvailableGpuMemory(), CalcAvailableClamped(bytes_expected));
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

  std::vector<GpuMemoryAllocation::BufferAllocation>
      suggested_buffer_allocation_values;
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasFrontbuffer);
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasFrontbuffer);
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasNoFrontbuffer);
  suggested_buffer_allocation_values.push_back(
      GpuMemoryAllocation::kHasNoFrontbuffer);

  for(size_t i = 0; i != gpu_resource_size_in_bytes_values.size(); ++i) {
    for(size_t j = 0; j != suggested_buffer_allocation_values.size(); ++j) {
      int sz = gpu_resource_size_in_bytes_values[i];
      GpuMemoryAllocation::BufferAllocation buffer_allocation =
          suggested_buffer_allocation_values[j];
      GpuMemoryAllocation allocation(sz, buffer_allocation);

      EXPECT_TRUE(allocation.Equals(
          GpuMemoryAllocation(sz, buffer_allocation)));
      EXPECT_FALSE(allocation.Equals(
          GpuMemoryAllocation(sz+1, buffer_allocation)));

      for(size_t k = 0; k != suggested_buffer_allocation_values.size(); ++k) {
        GpuMemoryAllocation::BufferAllocation buffer_allocation_other =
            suggested_buffer_allocation_values[k];
        if (buffer_allocation == buffer_allocation_other) continue;
        EXPECT_FALSE(allocation.Equals(
            GpuMemoryAllocation(sz, buffer_allocation_other)));
      }
    }
  }
}

// Test GpuMemoryManager Stub Memory Stats functionality:
// Creats various surface/non-surface stubs and switches stub visibility and
// tests to see that stats data structure values are correct.
TEST_F(GpuMemoryManagerTest, StubMemoryStatsForLastManageTests) {
  ClientAssignmentCollector::ClientMemoryStatMap stats;

  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  EXPECT_EQ(stats.size(), 0ul);

  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_);
  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  size_t stub1allocation1 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 1ul);
  EXPECT_GT(stub1allocation1, 0ul);

  FakeClient stub2(memmgr_);
  stub2.SetInSameShareGroup(&stub1);
  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  EXPECT_EQ(stats.count(&stub1), 1ul);
  size_t stub1allocation2 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;
  EXPECT_EQ(stats.count(&stub2), 1ul);
  size_t stub2allocation2 =
      stats[&stub2].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 2ul);
  EXPECT_GT(stub1allocation2, 0ul);
  EXPECT_GT(stub2allocation2, 0ul);
  if (stub1allocation2 != GetMaximumTabAllocation())
    EXPECT_LT(stub1allocation2, stub1allocation1);

  FakeClient stub3(memmgr_, GenerateUniqueSurfaceId(), true, older_);
  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  size_t stub1allocation3 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;
  size_t stub2allocation3 =
      stats[&stub2].allocation.renderer_allocation.bytes_limit_when_visible;
  size_t stub3allocation3 =
      stats[&stub3].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 3ul);
  EXPECT_GT(stub1allocation3, 0ul);
  EXPECT_GT(stub2allocation3, 0ul);
  EXPECT_GT(stub3allocation3, 0ul);
  if (stub1allocation3 != GetMaximumTabAllocation())
    EXPECT_LT(stub1allocation3, stub1allocation2);

  memmgr_.TestingSetClientVisible(&stub1, false);

  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  size_t stub1allocation4 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;
  size_t stub2allocation4 =
      stats[&stub2].allocation.renderer_allocation.bytes_limit_when_visible;
  size_t stub3allocation4 =
      stats[&stub3].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 3ul);
  EXPECT_GT(stub1allocation4, 0ul);
  EXPECT_GE(stub2allocation4, 0ul);
  EXPECT_GT(stub3allocation4, 0ul);
  if (stub3allocation3 != GetMaximumTabAllocation())
    EXPECT_GT(stub3allocation4, stub3allocation3);
}

// Test GpuMemoryManager's managed memory tracking
TEST_F(GpuMemoryManagerTest, TestManagedUsageTracking) {
  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_),
             stub2(memmgr_, GenerateUniqueSurfaceId(), false, older_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Set memory allocations and verify the results are reflected.
  memmgr_.SetClientManagedMemoryStats(
     &stub1, GpuManagedMemoryStats(0, 0, 5, false));
  memmgr_.SetClientManagedMemoryStats(
     &stub2, GpuManagedMemoryStats(0, 0, 7, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Redundantly add a client and make sure nothing changes
  memmgr_.AddClient(&stub1, true, true, older_);
  memmgr_.AddClient(&stub2, true, true, older_);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Remove a visible client
  memmgr_.RemoveClient(&stub1);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);
  memmgr_.SetClientManagedMemoryStats(
     &stub1, GpuManagedMemoryStats(0, 0, 99, false));
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);
  memmgr_.AddClient(&stub1, true, true, older_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);
  memmgr_.SetClientManagedMemoryStats(
     &stub1, GpuManagedMemoryStats(0, 0, 5, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Remove a backgrounded client
  memmgr_.RemoveClient(&stub2);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_backgrounded_);
  memmgr_.SetClientManagedMemoryStats(
     &stub2, GpuManagedMemoryStats(0, 0, 99, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_backgrounded_);
  memmgr_.AddClient(&stub2, true, false, older_);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_backgrounded_);
  memmgr_.SetClientManagedMemoryStats(
     &stub2, GpuManagedMemoryStats(0, 0, 7, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Create and then destroy some stubs, and verify their allocations go away.
  {
    FakeClient stub3(memmgr_, GenerateUniqueSurfaceId(), true, older_),
               stub4(memmgr_, GenerateUniqueSurfaceId(), false, older_);
    memmgr_.SetClientManagedMemoryStats(
       &stub3, GpuManagedMemoryStats(0, 0, 1, false));
    memmgr_.SetClientManagedMemoryStats(
       &stub4, GpuManagedMemoryStats(0, 0, 2, false));
    EXPECT_EQ(6ul, memmgr_.bytes_allocated_managed_visible_);
    EXPECT_EQ(9ul, memmgr_.bytes_allocated_managed_backgrounded_);
  }
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Do no-op changes to stubs' visibility and make sure nothing chnages.
  memmgr_.SetClientVisible(&stub1, true);
  memmgr_.SetClientVisible(&stub2, false);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Change visbility state.
  memmgr_.SetClientVisible(&stub1, false);
  memmgr_.SetClientVisible(&stub2, true);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Increase allocation amounts.
  memmgr_.SetClientManagedMemoryStats(
     &stub1, GpuManagedMemoryStats(0, 0, 6, false));
  memmgr_.SetClientManagedMemoryStats(
     &stub2, GpuManagedMemoryStats(0, 0, 8, false));
  EXPECT_EQ(8ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(6ul, memmgr_.bytes_allocated_managed_backgrounded_);

  // Decrease allocation amounts.
  memmgr_.SetClientManagedMemoryStats(
     &stub1, GpuManagedMemoryStats(0, 0, 4, false));
  memmgr_.SetClientManagedMemoryStats(
     &stub2, GpuManagedMemoryStats(0, 0, 6, false));
  EXPECT_EQ(6ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(4ul, memmgr_.bytes_allocated_managed_backgrounded_);
}

// Test GpuMemoryManager's background cutoff threshoulds
TEST_F(GpuMemoryManagerTest, TestBackgroundCutoff) {
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetBackgroundedAvailableGpuMemory(16);

  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_);

  // stub1's requirements are not <16, so it should just dump
  // everything when it goes invisible.
  stub1.SetUsageStats(16, 24, 18);
  Manage();
  EXPECT_EQ(0ul, stub1.BytesWhenNotVisible());

  // stub1 now fits, so it should have a full budget.
  stub1.SetUsageStats(15, 24, 18);
  Manage();
  EXPECT_EQ(memmgr_.bytes_backgrounded_available_gpu_memory_,
            memmgr_.GetCurrentBackgroundedAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());

  // background stub1
  stub1.SetUsageStats(15, 24, 15);
  stub1.SetVisible(false);

  // Add stub2 that uses almost enough memory to evict
  // stub1, but not quite
  FakeClient stub2(memmgr_, GenerateUniqueSurfaceId(), true, older_);
  stub2.SetUsageStats(15, 50, 48);
  Manage();
  EXPECT_EQ(memmgr_.bytes_backgrounded_available_gpu_memory_,
            memmgr_.GetCurrentBackgroundedAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());

  // Increase stub2 to break evict stub1
  stub2.SetUsageStats(15, 50, 49);
  Manage();
  EXPECT_EQ(0ul,
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());
}

// Test GpuMemoryManager's background MRU behavior
TEST_F(GpuMemoryManagerTest, TestBackgroundMru) {
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetBackgroundedAvailableGpuMemory(16);

  FakeClient stub1(memmgr_, GenerateUniqueSurfaceId(), true, older_);
  FakeClient stub2(memmgr_, GenerateUniqueSurfaceId(), true, older_);
  FakeClient stub3(memmgr_, GenerateUniqueSurfaceId(), true, older_);

  // When all are visible, they should all be allowed to have memory
  // should they become backgrounded.
  stub1.SetUsageStats(7, 24, 7);
  stub2.SetUsageStats(7, 24, 7);
  stub3.SetUsageStats(7, 24, 7);
  Manage();
  EXPECT_EQ(memmgr_.bytes_backgrounded_available_gpu_memory_,
            memmgr_.GetCurrentBackgroundedAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub3.BytesWhenNotVisible());


  // Background stubs 1 and 2, and they should fit
  stub2.SetVisible(false);
  // XXX - the MRU ordering is determined by taking the timestamp
  //       at which visibility changed. This does not have high
  //       enough resolution on all platforms to distinguish
  //       between the three SetVisible calls, so we have to
  //       explicitly set the timestamp.
  memmgr_.TestingSetClientLastUsedTime(&stub2, older_);
  stub1.SetVisible(false);
  memmgr_.TestingSetClientLastUsedTime(&stub1, newer_);
  Manage();
  EXPECT_EQ(memmgr_.bytes_backgrounded_available_gpu_memory_,
            memmgr_.GetCurrentBackgroundedAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub3.BytesWhenNotVisible());

  // Now background stub 3, and it should cause stub 2 to be
  // evicted because it was set non-visible first
  stub3.SetVisible(false);
  memmgr_.TestingSetClientLastUsedTime(&stub3, newer_);
  Manage();
  EXPECT_EQ(memmgr_.bytes_backgrounded_available_gpu_memory_,
            memmgr_.GetCurrentBackgroundedAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(0ul,
            stub2.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentBackgroundedAvailableGpuMemory(),
            stub3.BytesWhenNotVisible());
}

}  // namespace content
