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
  uint64 operator()(content::GpuMemoryManagerClient* ptr) const {
    return hash<uint64>()(reinterpret_cast<uint64>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

class FakeMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  virtual void TrackMemoryAllocatedChange(
      size_t /* old_size */,
      size_t /* new_size */,
      gpu::gles2::MemoryTracker::Pool /* pool */) OVERRIDE {
  }
  virtual bool EnsureGPUMemoryAvailable(size_t /* size_needed */) OVERRIDE {
    return true;
  }
 private:
  virtual ~FakeMemoryTracker() {
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
  uint64 total_gpu_memory_;
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

  virtual ~FakeClient() {
    client_state_.reset();
    tracking_group_.reset();
    memory_tracker_ = NULL;
  }

  virtual void SetMemoryAllocation(const GpuMemoryAllocation& alloc) OVERRIDE {
    allocation_ = alloc;
    ClientAssignmentCollector::AddClientStat(this, alloc);
  }

  virtual bool GetTotalGpuMemory(uint64* bytes) OVERRIDE {
    if (total_gpu_memory_) {
      *bytes = total_gpu_memory_;
      return true;
    }
    return false;
  }
  void SetTotalGpuMemory(uint64 bytes) { total_gpu_memory_ = bytes; }

  virtual gpu::gles2::MemoryTracker* GetMemoryTracker() const OVERRIDE {
    if (share_group_)
      return share_group_->GetMemoryTracker();
    return memory_tracker_.get();
  }

  virtual gfx::Size GetSurfaceSize() const OVERRIDE {
    return surface_size_;
  }
  void SetSurfaceSize(gfx::Size size) { surface_size_ = size; }

  void SetVisible(bool visible) {
    client_state_->SetVisible(visible);
  }

  void SetManagedMemoryStats(const GpuManagedMemoryStats& stats) {
    client_state_->SetManagedMemoryStats(stats);
  }

  uint64 BytesWhenVisible() const {
    return allocation_.renderer_allocation.bytes_limit_when_visible;
  }

  uint64 BytesWhenNotVisible() const {
    return allocation_.renderer_allocation.bytes_limit_when_not_visible;
  }
};

class GpuMemoryManagerTest : public testing::Test {
 protected:
  static const uint64 kFrontbufferLimitForTest = 3;

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
           !alloc.renderer_allocation.have_backbuffer_when_not_visible;
  }
  bool IsAllocationBackgroundForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible;
  }
  bool IsAllocationHibernatedForSurfaceYes(
      const GpuMemoryAllocation& alloc) {
    return !alloc.browser_allocation.suggest_have_frontbuffer &&
           !alloc.renderer_allocation.have_backbuffer_when_not_visible;
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

  uint64 CalcAvailableFromGpuTotal(uint64 bytes) {
    return GpuMemoryManager::CalcAvailableFromGpuTotal(bytes);
  }

  uint64 CalcAvailableClamped(uint64 bytes) {
    bytes = std::max(bytes, memmgr_.GetDefaultAvailableGpuMemory());
    bytes = std::min(bytes, memmgr_.GetMaximumTotalGpuMemory());
    return bytes;
  }

  uint64 GetAvailableGpuMemory() {
    return memmgr_.GetAvailableGpuMemory();
  }

  uint64 GetMaximumClientAllocation() {
    return memmgr_.GetMaximumClientAllocation();
  }

  uint64 GetMinimumClientAllocation() {
    return memmgr_.GetMinimumClientAllocation();
  }

  void SetClientStats(
      FakeClient* client,
      uint64 required,
      uint64 nicetohave) {
    client->SetManagedMemoryStats(
        GpuManagedMemoryStats(required, nicetohave, 0, false));
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
  size_t max_stubs_before_no_bonus = static_cast<size_t>(
      GetAvailableGpuMemory() / (GetMinimumClientAllocation() + 1));

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
        GetMinimumClientAllocation());
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
  // We take the lowest GPU's total memory as the limit
  uint64 expected = 400 * 1024 * 1024;
  stub1.SetTotalGpuMemory(expected); // GPU Memory
  stub2.SetTotalGpuMemory(expected - 1024 * 1024); // Smaller but not visible.
  stub3.SetTotalGpuMemory(expected + 1024 * 1024); // Visible but larger.
  stub4.SetTotalGpuMemory(expected + 1024 * 1024); // Not visible and larger.
  Manage();
  uint64 bytes_expected = CalcAvailableFromGpuTotal(expected);
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

  for (size_t i = 0; i != gpu_resource_size_in_bytes_values.size(); ++i) {
    for (size_t j = 0; j != suggested_buffer_allocation_values.size(); ++j) {
      uint64 sz = gpu_resource_size_in_bytes_values[i];
      GpuMemoryAllocation::BufferAllocation buffer_allocation =
          suggested_buffer_allocation_values[j];
      GpuMemoryAllocation allocation(sz, buffer_allocation);

      EXPECT_TRUE(allocation.Equals(
          GpuMemoryAllocation(sz, buffer_allocation)));
      EXPECT_FALSE(allocation.Equals(
          GpuMemoryAllocation(sz+1, buffer_allocation)));

      for (size_t k = 0; k != suggested_buffer_allocation_values.size(); ++k) {
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
  uint64 stub1allocation1 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 1ul);
  EXPECT_GT(stub1allocation1, 0ul);

  FakeClient stub2(&memmgr_, &stub1);
  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  EXPECT_EQ(stats.count(&stub1), 1ul);
  uint64 stub1allocation2 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;
  EXPECT_EQ(stats.count(&stub2), 1ul);
  uint64 stub2allocation2 =
      stats[&stub2].allocation.renderer_allocation.bytes_limit_when_visible;

  EXPECT_EQ(stats.size(), 2ul);
  EXPECT_GT(stub1allocation2, 0ul);
  EXPECT_GT(stub2allocation2, 0ul);
  if (stub1allocation2 != GetMaximumClientAllocation())
    EXPECT_LT(stub1allocation2, stub1allocation1);

  FakeClient stub3(&memmgr_, GenerateUniqueSurfaceId(), true);
  Manage();
  stats = ClientAssignmentCollector::GetClientStatsForLastManage();
  uint64 stub1allocation3 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;
  uint64 stub2allocation3 =
      stats[&stub2].allocation.renderer_allocation.bytes_limit_when_visible;
  uint64 stub3allocation3 =
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
  uint64 stub1allocation4 =
      stats[&stub1].allocation.renderer_allocation.bytes_limit_when_visible;
  uint64 stub2allocation4 =
      stats[&stub2].allocation.renderer_allocation.bytes_limit_when_visible;
  uint64 stub3allocation4 =
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

// Test nonvisible MRU behavior (the most recently used nonvisible clients
// keep their contents).
TEST_F(GpuMemoryManagerTest, BackgroundMru) {
  // Set memory manager constants for this test. Note that the budget
  // for backgrounded content will be 64/4 = 16.
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetMinimumClientAllocation(8);

  uint64 bytes_when_not_visible_expected = 6u;
#if defined (OS_ANDROID)
  bytes_when_not_visible_expected = 0;
#endif

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);
  FakeClient stub2(&memmgr_, GenerateUniqueSurfaceId(), true);
  FakeClient stub3(&memmgr_, GenerateUniqueSurfaceId(), true);

  // When all are visible, they should all be allowed to have memory
  // should they become nonvisible.
  SetClientStats(&stub1, 6, 23);
  SetClientStats(&stub2, 6, 23);
  SetClientStats(&stub3, 6, 23);
  Manage();
  EXPECT_GE(stub1.BytesWhenVisible(), 20u);
  EXPECT_GE(stub2.BytesWhenVisible(), 20u);
  EXPECT_GE(stub3.BytesWhenVisible(), 20u);
  EXPECT_LT(stub1.BytesWhenVisible(), 22u);
  EXPECT_LT(stub2.BytesWhenVisible(), 22u);
  EXPECT_LT(stub3.BytesWhenVisible(), 22u);
  EXPECT_GE(stub1.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  EXPECT_GE(stub2.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  EXPECT_GE(stub3.BytesWhenNotVisible(), bytes_when_not_visible_expected);

  // Background stubs 1 and 2, and they should fit. All stubs should
  // have their full nicetohave budget should they become visible.
  stub2.SetVisible(false);
  stub1.SetVisible(false);
  Manage();
  EXPECT_GE(stub1.BytesWhenVisible(), 23u);
  EXPECT_GE(stub2.BytesWhenVisible(), 23u);
  EXPECT_GE(stub3.BytesWhenVisible(), 23u);
  EXPECT_LT(stub1.BytesWhenVisible(), 32u);
  EXPECT_LT(stub2.BytesWhenVisible(), 32u);
  EXPECT_GE(stub1.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  EXPECT_GE(stub2.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  EXPECT_GE(stub3.BytesWhenNotVisible(), bytes_when_not_visible_expected);

  // Now background stub 3, and it should cause stub 2 to be
  // evicted because it was set non-visible first
  stub3.SetVisible(false);
  Manage();
  EXPECT_GE(stub1.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  EXPECT_EQ(stub2.BytesWhenNotVisible(), 0u);
  EXPECT_GE(stub3.BytesWhenNotVisible(), bytes_when_not_visible_expected);
}

// Test that once a backgrounded client has dropped its resources, it
// doesn't get them back until it becomes visible again.
TEST_F(GpuMemoryManagerTest, BackgroundDiscardPersistent) {
  // Set memory manager constants for this test. Note that the budget
  // for backgrounded content will be 64/4 = 16.
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetMinimumClientAllocation(8);

  uint64 bytes_when_not_visible_expected = 10ul;
#if defined (OS_ANDROID)
  bytes_when_not_visible_expected = 0;
#endif

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);
  FakeClient stub2(&memmgr_, GenerateUniqueSurfaceId(), true);

  // Both clients should be able to keep their contents should one of
  // them become nonvisible.
  SetClientStats(&stub1, 10, 20);
  SetClientStats(&stub2, 10, 20);
  Manage();
  EXPECT_GE(stub1.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  EXPECT_GE(stub2.BytesWhenNotVisible(), bytes_when_not_visible_expected);

  // If they both go nonvisible, then only the most recently used client
  // should keep its contents.
  stub1.SetVisible(false);
  stub2.SetVisible(false);
  Manage();
  EXPECT_EQ(stub1.BytesWhenNotVisible(), 0u);
  EXPECT_GE(stub2.BytesWhenNotVisible(), bytes_when_not_visible_expected);

  // When becoming visible, stub 2 should get its contents back, and
  // retain them next time it is made nonvisible.
  stub2.SetVisible(true);
  Manage();
  EXPECT_GE(stub2.BytesWhenNotVisible(), bytes_when_not_visible_expected);
  stub2.SetVisible(false);
  Manage();
  EXPECT_GE(stub2.BytesWhenNotVisible(), bytes_when_not_visible_expected);
}

// Test tracking of unmanaged (e.g, WebGL) memory.
TEST_F(GpuMemoryManagerTest, UnmanagedTracking) {
  // Set memory manager constants for this test
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetMinimumClientAllocation(8);
  memmgr_.TestingSetUnmanagedLimitStep(16);

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);

  // Expect that the one stub get its nicetohave level.
  SetClientStats(&stub1, 16, 32);
  Manage();
  EXPECT_GE(stub1.BytesWhenVisible(), 32u);

  // Now allocate some unmanaged memory and make sure the amount
  // goes down.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      0,
      48,
      gpu::gles2::MemoryTracker::kUnmanaged);
  Manage();
  EXPECT_LT(stub1.BytesWhenVisible(), 24u);

  // Now allocate the entire FB worth of unmanaged memory, and
  // make sure that we stay stuck at the minimum tab allocation.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      48,
      64,
      gpu::gles2::MemoryTracker::kUnmanaged);
  Manage();
  EXPECT_EQ(stub1.BytesWhenVisible(), 8u);

  // Far-oversubscribe the entire FB, and make sure we stay at
  // the minimum allocation, and don't blow up.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      64,
      999,
      gpu::gles2::MemoryTracker::kUnmanaged);
  Manage();
  EXPECT_EQ(stub1.BytesWhenVisible(), 8u);

  // Delete all tracked memory so we don't hit leak checks.
  memmgr_.TrackMemoryAllocatedChange(
      stub1.tracking_group_.get(),
      999,
      0,
      gpu::gles2::MemoryTracker::kUnmanaged);
}

// Test the default allocation levels are used.
TEST_F(GpuMemoryManagerTest, DefaultAllocation) {
  // Set memory manager constants for this test
  memmgr_.TestingSetAvailableGpuMemory(64);
  memmgr_.TestingSetMinimumClientAllocation(8);
  memmgr_.TestingSetDefaultClientAllocation(16);

  FakeClient stub1(&memmgr_, GenerateUniqueSurfaceId(), true);

  // Expect that a client which has not sent stats receive at
  // least the default allocation.
  Manage();
  EXPECT_GE(stub1.BytesWhenVisible(),
            memmgr_.GetDefaultClientAllocation());
  EXPECT_EQ(stub1.BytesWhenNotVisible(), 0u);
}

}  // namespace content
