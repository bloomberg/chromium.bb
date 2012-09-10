// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestObserver : public content::GpuDataManagerObserver {
 public:
  TestObserver()
      : gpu_info_updated_(false),
        video_memory_usage_stats_updated_(false) {
  }
  virtual ~TestObserver() { }

  bool gpu_info_updated() const { return gpu_info_updated_; }
  bool video_memory_usage_stats_updated() const {
    return video_memory_usage_stats_updated_;
  }

  virtual void OnGpuInfoUpdate() OVERRIDE {
    gpu_info_updated_ = true;
  }

  virtual void OnVideoMemoryUsageStatsUpdate(
      const content::GPUVideoMemoryUsageStats& stats) OVERRIDE {
    video_memory_usage_stats_updated_ = true;
  }

 private:
  bool gpu_info_updated_;
  bool video_memory_usage_stats_updated_;
};

}  // namespace anonymous

class GpuDataManagerImplTest : public testing::Test {
 public:
  GpuDataManagerImplTest() { }

  virtual ~GpuDataManagerImplTest() { }

 protected:
  void SetUp() {
  }

  void TearDown() {
  }

  MessageLoop message_loop_;
};

// We use new method instead of GetInstance() method because we want
// each test to be independent of each other.

TEST_F(GpuDataManagerImplTest, GpuSideBlacklisting) {
  // If a feature is allowed in preliminary step (browser side), but
  // disabled when GPU process launches and collects full GPU info,
  // it's too late to let renderer know, so we basically block all GPU
  // access, to be on the safe side.
  GpuDataManagerImpl* manager = new GpuDataManagerImpl();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  manager->UpdateBlacklistedFeatures(
      content::GPU_FEATURE_TYPE_WEBGL);
  manager->UpdatePreliminaryBlacklistedFeatures();
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(content::GPU_FEATURE_TYPE_WEBGL,
            manager->GetBlacklistedFeatures());

  manager->UpdateBlacklistedFeatures(
      content::GPU_FEATURE_TYPE_WEBGL);
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(content::GPU_FEATURE_TYPE_WEBGL,
            manager->GetBlacklistedFeatures());

  manager->UpdateBlacklistedFeatures(
      content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  EXPECT_EQ(content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
            manager->GetBlacklistedFeatures());
  EXPECT_FALSE(manager->GpuAccessAllowed());

  delete manager;
}

TEST_F(GpuDataManagerImplTest, BlacklistCard) {
  GpuDataManagerImpl* manager = new GpuDataManagerImpl();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  manager->BlacklistCard();
  EXPECT_FALSE(manager->GpuAccessAllowed());

  // If software rendering is enabled, even if we blacklist GPU,
  // GPU process is still allowed.
  manager->software_rendering_ = true;
  EXPECT_TRUE(manager->GpuAccessAllowed());

  delete manager;
}

TEST_F(GpuDataManagerImplTest, GpuInfoUpdate) {
  GpuDataManagerImpl* manager = new GpuDataManagerImpl();
  ASSERT_TRUE(manager);

  TestObserver observer;
  manager->AddObserver(&observer);

  EXPECT_FALSE(observer.gpu_info_updated());

  content::GPUInfo gpu_info;
  manager->UpdateGpuInfo(gpu_info);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(observer.gpu_info_updated());

  delete manager;
}

TEST_F(GpuDataManagerImplTest, GPUVideoMemoryUsageStatsUpdate) {
  GpuDataManagerImpl* manager = new GpuDataManagerImpl();
  ASSERT_TRUE(manager);

  TestObserver observer;
  manager->AddObserver(&observer);

  EXPECT_FALSE(observer.video_memory_usage_stats_updated());

  content::GPUVideoMemoryUsageStats vram_stats;
  manager->UpdateVideoMemoryUsageStats(vram_stats);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_TRUE(observer.video_memory_usage_stats_updated());

  delete manager;
}

