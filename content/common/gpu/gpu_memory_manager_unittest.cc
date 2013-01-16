// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_allocation.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "ui/gfx/size_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<content::GpuMemoryManagerClient*> {
  size_t operator()(content::GpuMemoryManagerClient* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

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
  GpuMemoryManager* memmgr_;
  GpuMemoryAllocation allocation_;
  size_t total_gpu_memory_;
  gfx::Size surface_size_;
  GpuMemoryManagerClient* share_group_;
  scoped_refptr<gpu::gles2::MemoryTracker> memory_tracker_;
  scoped_ptr<GpuMemoryTrackingGroup> tracking_group_;
  scoped_ptr<GpuMemoryManagerClientState> client_state_;

  // This will create a client with no surface
  FakeClient(GpuMemoryManager* memmgr, GpuMemoryManagerClient* share_group)
      : memmgr_(memmgr)
      , total_gpu_memory_(0)
      , share_group_(share_group)
      , memory_tracker_(NULL)
      , tracking_group_(NULL) {
    if (!share_group_) {
      memory_tracker_ = new FakeMemoryTracker();
      tracking_group_.reset(
          memmgr_->CreateTrackingGroup(0, memory_tracker_));
    }
    client_state_.reset(memmgr_->CreateClientState(this, false, true));
  }

  // This will create a client with a surface
  FakeClient(GpuMemoryManager* memmgr,
             int32 surface_id,
             bool visible)
      : memmgr_(memmgr)
      , total_gpu_memory_(0)
      , share_group_(NULL)
      , memory_tracker_(NULL)
      , tracking_group_(NULL) {
    memory_tracker_ = new FakeMemoryTracker();
    tracking_group_.reset(
        memmgr_->CreateTrackingGroup(0, memory_tracker_));
    client_state_.reset(memmgr_->CreateClientState(
        this, surface_id != 0, visible));
  }

  ~FakeClient() {
    client_state_.reset();
    tracking_group_.reset();
    memory_tracker_ = NULL;
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
    if (share_group_)
      return share_group_->GetMemoryTracker();
    return memory_tracker_.get();
  }

  gfx::Size GetSurfaceSize() const {
    return surface_size_;
  }
  void SetSurfaceSize(gfx::Size size) { surface_size_ = size; }

  void SetVisible(bool visible) {
    client_state_->SetVisible(visible);
  }

  void SetManagedMemoryStats(const GpuManagedMemoryStats& stats) {
    client_state_->SetManagedMemoryStats(stats);
  }

  size_t BytesWhenVisible() const {
    return allocation_.renderer_allocation.bytes_limit_when_visible;
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
               GetMinimumClientAllocation();
  }
  bool IsAllocationBackgroundForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_not_visible <=
               memmgr_.GetCurrentNonvisibleAvailableGpuMemory();
  }
  bool IsAllocationHibernatedForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_not_visible <=
               memmgr_.GetCurrentNonvisibleAvailableGpuMemory();
  }
  bool IsAllocationForegroundForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_visible ==
               GetMinimumClientAllocation();
  }
  bool IsAllocationBackgroundForSurfaceNo(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible &&
           alloc.renderer_allocation.bytes_limit_when_visible ==
               GetMinimumClientAllocation();
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

  size_t GetMaximumClientAllocation() {
    return memmgr_.GetMaximumClientAllocation();
  }

  size_t GetMinimumClientAllocation() {
    return memmgr_.GetMinimumClientAllocation();
  }

  GpuMemoryManager memmgr_;
};

// Test GpuMemoryManager::Manage basic functionality.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer
// according to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageBasicFunctionality) {
  // Test stubs with surface.
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));

  // Test stubs without surface, with share group of 1 stub.
  FakeClient stub3(&memmgr_, &stub1), stub4(&memmgr_, &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // Test stub without surface, with share group of multiple stubs.
  FakeClient stub5(&memmgr_ , &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));
}

// Test GpuMemoryManager::Manage functionality: changing visibility.
// Expect memory allocation to set suggest_have_frontbuffer/backbuffer
// according to visibility and last used time for stubs with surface.
// Expect memory allocation to be shared according to share groups for stubs
// without a surface.
TEST_F(GpuMemoryManagerTest, TestManageChangingVisibility) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);

  FakeClient stub3(&memmgr_, &stub1), stub4(&memmgr_, &stub2);
  FakeClient stub5(&memmgr_ , &stub2);

  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub5.allocation_));

  stub1.SetVisible(false);
  stub2.SetVisible(true);

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
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), true);

  FakeClient stub5(&memmgr_ , &stub1), stub6(&memmgr_ , &stub2);
  FakeClient stub7(&memmgr_ , &stub2);

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
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), true);
  stub4.SetVisible(false);
  stub3.SetVisible(false);
  stub2.SetVisible(false);
  stub1.SetVisible(false);

  FakeClient stub5(&memmgr_ , &stub1), stub6(&memmgr_ , &stub4);
  FakeClient stub7(&memmgr_ , &stub1);

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
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), true);

  FakeClient stub5(&memmgr_ , &stub3), stub6(&memmgr_ , &stub4);
  FakeClient stub7(&memmgr_ , &stub3);

  // Make stub4 be the least-recently-used client
  stub4.SetVisible(false);
  stub3.SetVisible(false);
  stub2.SetVisible(false);
  stub1.SetVisible(false);

  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub4.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub5.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub6.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub7.allocation_));

  // Make stub3 become the least-recently-used client.
  stub2.SetVisible(true);
  stub2.SetVisible(false);
  stub4.SetVisible(true);
  stub4.SetVisible(false);

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
  FakeClient stub_ignore_a(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub_ignore_b(&memmgr_, GenerateUniqueSurfaceId(), false),
             stub_ignore_c(&memmgr_, GenerateUniqueSurfaceId(), false);
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), false),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);

  FakeClient stub3(&memmgr_, &stub2), stub4(&memmgr_, &stub2);

  // stub1 and stub2 keep their non-hibernated state because they're
  // either visible or the 2 most recently used clients (through the
  // first three checks).
  stub1.SetVisible(true);
  stub2.SetVisible(true);
  Manage();
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  stub1.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationForegroundForSurfaceNo(stub4.allocation_));

  stub2.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // stub_ignore_b will cause stub1 to become hibernated (because
  // stub_ignore_a, stub_ignore_b, and stub2 are all non-hibernated and more
  // important).
  stub_ignore_b.SetVisible(true);
  stub_ignore_b.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationBackgroundForSurfaceNo(stub4.allocation_));

  // stub_ignore_c will cause stub2 to become hibernated (because
  // stub_ignore_a, stub_ignore_b, and stub_ignore_c are all non-hibernated
  // and more important).
  stub_ignore_c.SetVisible(true);
  stub_ignore_c.SetVisible(false);
  Manage();
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub1.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceYes(stub2.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub3.allocation_));
  EXPECT_TRUE(IsAllocationHibernatedForSurfaceNo(stub4.allocation_));
}

// Test GpuMemoryAllocation memory allocation bonuses:
// When the number of visible tabs is small, each tab should get a
// gpu_resource_size_in_bytes allocation value that is greater than
// GetMinimumClientAllocation(), and when the number of tabs is large,
// each should get exactly GetMinimumClientAllocation() and not less.
TEST_F(GpuMemoryManagerTest, TestForegroundStubsGetBonusAllocation) {
  size_t max_stubs_before_no_bonus =
      GetAvailableGpuMemory() / (GetMinimumClientAllocation() + 1);

  std::vector<FakeClient*> stubs;
  for (size_t i = 0; i < max_stubs_before_no_bonus; ++i) {
    stubs.push_back(
        new FakeClient(&memmgr_, GenerateUniqueSurfaceId(), true));
  }

  Manage();
  for (size_t i = 0; i < stubs.size(); ++i) {
    EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stubs[i]->allocation_));
    EXPECT_GT(
        stubs[i]->allocation_.renderer_allocation.bytes_limit_when_visible,
        static_cast<size_t>(GetMinimumClientAllocation()));
  }

  FakeClient extra_stub(&memmgr_, GenerateUniqueSurfaceId(), true);

  Manage();
  for (size_t i = 0; i < stubs.size(); ++i) {
    EXPECT_TRUE(IsAllocationForegroundForSurfaceYes(stubs[i]->allocation_));
    EXPECT_EQ(
        stubs[i]->allocation_.renderer_allocation.bytes_limit_when_visible,
        GetMinimumClientAllocation());
  }

  for (size_t i = 0; i < max_stubs_before_no_bonus; ++i) {
    delete stubs[i];
  }
}

// Test GpuMemoryManager::UpdateAvailableGpuMemory functionality
TEST_F(GpuMemoryManagerTest, TestUpdateAvailableGpuMemory) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false),
             stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub4(&memmgr_, GenerateUniqueSurfaceId(), false);

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
// Creates various surface/non-surface stubs and switches stub visibility and
// tests to see that stats data structure values are correct.
TEST_F(GpuMemoryManagerTest, StubMemoryStatsForLastManageTests) {
  ClientAssignmentCollector::ClientMemoryStatMap stats;

  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  EXPECT_EQ(stats.size(), 0ul);

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);
  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  size_t stub1allocation1 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 1ul);
  EXPECT_GT(stub1allocation1, 0ul);

  FakeClient stub2(&memmgr_, &stub1);
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
  if (stub1allocation2 != GetMaximumClientAllocation())
    EXPECT_LT(stub1allocation2, stub1allocation1);

  FakeClient stub3(&memmgr_, GenerateUniqueSurfaceId(), true);
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
  if (stub1allocation3 != GetMaximumClientAllocation())
    EXPECT_LT(stub1allocation3, stub1allocation2);

  stub1.SetVisible(false);

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
  if (stub3allocation3 != GetMaximumClientAllocation())
    EXPECT_GT(stub3allocation4, stub3allocation3);
}

// Test GpuMemoryManager's managed memory tracking
TEST_F(GpuMemoryManagerTest, TestManagedUsageTracking) {
  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true),
             stub2(&memmgr_, GenerateUniqueSurfaceId(), false);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Set memory allocations and verify the results are reflected.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 5, false));
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 7, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Remove a visible client
  stub1.client_state_.reset();
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);
  stub1.client_state_.reset(memmgr_.CreateClientState(&stub1, true, true));
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 5, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Remove a nonvisible client
  stub2.client_state_.reset();
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_nonvisible_);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_nonvisible_);
  stub2.client_state_.reset(memmgr_.CreateClientState(&stub2, true, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(0ul, memmgr_.bytes_allocated_managed_nonvisible_);
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 7, false));
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Create and then destroy some stubs, and verify their allocations go away.
  {
    FakeClient stub3(&memmgr_, GenerateUniqueSurfaceId(), true),
               stub4(&memmgr_, GenerateUniqueSurfaceId(), false);
    stub3.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 1, false));
    stub4.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 2, false));
    EXPECT_EQ(6ul, memmgr_.bytes_allocated_managed_visible_);
    EXPECT_EQ(9ul, memmgr_.bytes_allocated_managed_nonvisible_);
  }
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Do no-op changes to stubs' visibility and make sure nothing chnages.
  stub1.SetVisible(true);
  stub2.SetVisible(false);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Change visbility state.
  stub1.SetVisible(false);
  stub2.SetVisible(true);
  EXPECT_EQ(7ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(5ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Increase allocation amounts.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 6, false));
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 8, false));
  EXPECT_EQ(8ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(6ul, memmgr_.bytes_allocated_managed_nonvisible_);

  // Decrease allocation amounts.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 4, false));
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(0, 0, 6, false));
  EXPECT_EQ(6ul, memmgr_.bytes_allocated_managed_visible_);
  EXPECT_EQ(4ul, memmgr_.bytes_allocated_managed_nonvisible_);
}

// Test GpuMemoryManager's background cutoff threshoulds
TEST_F(GpuMemoryManagerTest, TestBackgroundCutoff) {
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetNonvisibleAvailableGpuMemory(16);

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);

  // stub1's requirements are not <=16, so it should just dump
  // everything when it goes invisible.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(17, 24, 18, false));
  Manage();
  EXPECT_EQ(0ul, stub1.BytesWhenNotVisible());

  // stub1 now fits, so it should have a full budget.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(16, 24, 18, false));
  Manage();
  EXPECT_EQ(memmgr_.bytes_nonvisible_available_gpu_memory_,
            memmgr_.GetCurrentNonvisibleAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());

  // Background stub1.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(16, 24, 16, false));
  stub1.SetVisible(false);

  // Add stub2 that uses almost enough memory to evict
  // stub1, but not quite.
  FakeClient stub2(&memmgr_, GenerateUniqueSurfaceId(), true);
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(16, 50, 48, false));
  Manage();
  EXPECT_EQ(memmgr_.bytes_nonvisible_available_gpu_memory_,
            memmgr_.GetCurrentNonvisibleAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());

  // Increase stub2 to force stub1 to be evicted.
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(16, 50, 49, false));
  Manage();
  EXPECT_EQ(0ul,
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());
}

// Test GpuMemoryManager's background MRU behavior
TEST_F(GpuMemoryManagerTest, TestBackgroundMru) {
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetNonvisibleAvailableGpuMemory(16);

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);
  FakeClient stub2(&memmgr_, GenerateUniqueSurfaceId(), true);
  FakeClient stub3(&memmgr_, GenerateUniqueSurfaceId(), true);

  // When all are visible, they should all be allowed to have memory
  // should they become nonvisible.
  stub1.SetManagedMemoryStats(GpuManagedMemoryStats(7, 24, 7, false));
  stub2.SetManagedMemoryStats(GpuManagedMemoryStats(7, 24, 7, false));
  stub3.SetManagedMemoryStats(GpuManagedMemoryStats(7, 24, 7, false));
  Manage();
  EXPECT_EQ(memmgr_.bytes_nonvisible_available_gpu_memory_,
            memmgr_.GetCurrentNonvisibleAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub3.BytesWhenNotVisible());


  // Background stubs 1 and 2, and they should fit
  stub2.SetVisible(false);
  stub1.SetVisible(false);
  Manage();
  EXPECT_EQ(memmgr_.bytes_nonvisible_available_gpu_memory_,
            memmgr_.GetCurrentNonvisibleAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub2.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub3.BytesWhenNotVisible());

  // Now background stub 3, and it should cause stub 2 to be
  // evicted because it was set non-visible first
  stub3.SetVisible(false);
  Manage();
  EXPECT_EQ(memmgr_.bytes_nonvisible_available_gpu_memory_,
            memmgr_.GetCurrentNonvisibleAvailableGpuMemory());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub1.BytesWhenNotVisible());
  EXPECT_EQ(0ul,
            stub2.BytesWhenNotVisible());
  EXPECT_EQ(memmgr_.GetCurrentNonvisibleAvailableGpuMemory(),
            stub3.BytesWhenNotVisible());
}

// Test GpuMemoryManager's tracking of unmanaged (e.g, WebGL) memory.
TEST_F(GpuMemoryManagerTest, TestUnmanagedTracking) {
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetNonvisibleAvailableGpuMemory(16);
  memmgr_.TestingSetUnmanagedLimitStep(16);
  memmgr_.TestingSetMinimumClientAllocation(8);

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);

  // Expect that the one stub get the maximum tab allocation.
  Manage();
  EXPECT_EQ(memmgr_.GetMaximumClientAllocation(),
            stub1.BytesWhenVisible());

  // Now allocate some unmanaged memory and make sure the amount
  // goes down.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      0,
      48,
      gpu::gles2::MemoryTracker::kUnmanaged);
  Manage();
  EXPECT_GT(memmgr_.GetMaximumClientAllocation(),
            stub1.BytesWhenVisible());

  // Now allocate the entire FB worth of unmanaged memory, and
  // make sure that we stay stuck at the minimum tab allocation.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      48,
      64,
      gpu::gles2::MemoryTracker::kUnmanaged);
  Manage();
  EXPECT_EQ(memmgr_.GetMinimumClientAllocation(),
            stub1.BytesWhenVisible());

  // Far-oversubscribe the entire FB, and make sure we stay at
  // the minimum allocation, and don't blow up.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      64,
      999,
      gpu::gles2::MemoryTracker::kUnmanaged);
  Manage();
  EXPECT_EQ(memmgr_.GetMinimumClientAllocation(),
            stub1.BytesWhenVisible());

  // Delete all tracked memory so we don't hit leak checks.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      999,
      0,
      gpu::gles2::MemoryTracker::kUnmanaged);
}

}  // namespace content
