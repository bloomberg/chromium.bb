// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_manager.h"

#include "content/common/gpu/gpu_memory_manager_client.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size_conversions.h"

using gpu::MemoryAllocation;

class FakeMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  void TrackMemoryAllocatedChange(
      size_t /* old_size */,
      size_t /* new_size */) override {}
  bool EnsureGPUMemoryAvailable(size_t /* size_needed */) override {
    return true;
  }
  uint64_t ClientTracingId() const override { return 0; }
  int ClientId() const override { return 0; }
  uint64_t ShareGroupTracingGUID() const override { return 0; }

 private:
  ~FakeMemoryTracker() override {}
};

namespace content {

// This class is used to collect all stub assignments during a
// Manage() call.
class ClientAssignmentCollector {
 public:
  struct ClientMemoryStat {
    MemoryAllocation allocation;
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
                          const MemoryAllocation& allocation) {
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
  bool suggest_have_frontbuffer_;
  MemoryAllocation allocation_;
  uint64 total_gpu_memory_;
  gfx::Size surface_size_;
  GpuMemoryManagerClient* share_group_;
  scoped_refptr<gpu::gles2::MemoryTracker> memory_tracker_;
  scoped_ptr<GpuMemoryTrackingGroup> tracking_group_;
  scoped_ptr<GpuMemoryManagerClientState> client_state_;

  // This will create a client with no surface
  FakeClient(GpuMemoryManager* memmgr, GpuMemoryManagerClient* share_group)
      : memmgr_(memmgr),
        suggest_have_frontbuffer_(false),
        total_gpu_memory_(0),
        share_group_(share_group),
        memory_tracker_(NULL) {
    if (!share_group_) {
      memory_tracker_ = new FakeMemoryTracker();
      tracking_group_.reset(
          memmgr_->CreateTrackingGroup(0, memory_tracker_.get()));
    }
    client_state_.reset(memmgr_->CreateClientState(this, false, true));
  }

  // This will create a client with a surface
  FakeClient(GpuMemoryManager* memmgr, int32 surface_id, bool visible)
      : memmgr_(memmgr),
        suggest_have_frontbuffer_(false),
        total_gpu_memory_(0),
        share_group_(NULL),
        memory_tracker_(NULL) {
    memory_tracker_ = new FakeMemoryTracker();
    tracking_group_.reset(
        memmgr_->CreateTrackingGroup(0, memory_tracker_.get()));
    client_state_.reset(
        memmgr_->CreateClientState(this, surface_id != 0, visible));
  }

  ~FakeClient() override {
    client_state_.reset();
    tracking_group_.reset();
    memory_tracker_ = NULL;
  }

  void SuggestHaveFrontBuffer(bool suggest_have_frontbuffer) override {
    suggest_have_frontbuffer_ = suggest_have_frontbuffer;
  }

  bool GetTotalGpuMemory(uint64* bytes) override {
    if (total_gpu_memory_) {
      *bytes = total_gpu_memory_;
      return true;
    }
    return false;
  }
  void SetTotalGpuMemory(uint64 bytes) { total_gpu_memory_ = bytes; }

  gpu::gles2::MemoryTracker* GetMemoryTracker() const override {
    if (share_group_)
      return share_group_->GetMemoryTracker();
    return memory_tracker_.get();
  }

  gfx::Size GetSurfaceSize() const override { return surface_size_; }
  void SetSurfaceSize(gfx::Size size) { surface_size_ = size; }

  void SetVisible(bool visible) {
    client_state_->SetVisible(visible);
  }

  uint64 BytesWhenVisible() const {
    return allocation_.bytes_limit_when_visible;
  }
};

class GpuMemoryManagerTest : public testing::Test {
 protected:
  static const uint64 kFrontbufferLimitForTest = 3;

  GpuMemoryManagerTest()
      : memmgr_(0, kFrontbufferLimitForTest) {
    memmgr_.TestingDisableScheduleManage();
  }

  void SetUp() override {}

  static int32 GenerateUniqueSurfaceId() {
    static int32 surface_id_ = 1;
    return surface_id_++;
  }

  bool IsAllocationForegroundForSurfaceYes(
      const MemoryAllocation& alloc) {
    return true;
  }
  bool IsAllocationBackgroundForSurfaceYes(
      const MemoryAllocation& alloc) {
    return true;
  }
  bool IsAllocationHibernatedForSurfaceYes(
      const MemoryAllocation& alloc) {
    return true;
  }
  bool IsAllocationForegroundForSurfaceNo(
      const MemoryAllocation& alloc) {
    return alloc.bytes_limit_when_visible != 0;
  }
  bool IsAllocationBackgroundForSurfaceNo(
      const MemoryAllocation& alloc) {
    return alloc.bytes_limit_when_visible != 0;
  }
  bool IsAllocationHibernatedForSurfaceNo(
      const MemoryAllocation& alloc) {
    return alloc.bytes_limit_when_visible == 0;
  }

  void Manage() {
    ClientAssignmentCollector::ClearAllStats();
    memmgr_.Manage();
  }

  GpuMemoryManager memmgr_;
};

}  // namespace content
