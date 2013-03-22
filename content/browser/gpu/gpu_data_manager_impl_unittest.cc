// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/time.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/gpu_info.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace content {
namespace {

class TestObserver : public GpuDataManagerObserver {
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
      const GPUVideoMemoryUsageStats& stats) OVERRIDE {
    video_memory_usage_stats_updated_ = true;
  }

 private:
  bool gpu_info_updated_;
  bool video_memory_usage_stats_updated_;
};

static base::Time GetTimeForTesting() {
  return base::Time::FromDoubleT(1000);
}

static GURL GetDomain1ForTesting() {
  return GURL("http://foo.com/");
}

static GURL GetDomain2ForTesting() {
  return GURL("http://bar.com/");
}

}  // namespace anonymous

class GpuDataManagerImplTest : public testing::Test {
 public:
  GpuDataManagerImplTest() { }

  virtual ~GpuDataManagerImplTest() { }

 protected:
  // scoped_ptr doesn't work with GpuDataManagerImpl because its
  // destructor is private. GpuDataManagerImplTest is however a friend
  // so we can make a little helper class here.
  class ScopedGpuDataManagerImpl {
   public:
    ScopedGpuDataManagerImpl() : impl_(new GpuDataManagerImpl()) {}
    ~ScopedGpuDataManagerImpl() { delete impl_; }

    GpuDataManagerImpl* get() const { return impl_; }
    GpuDataManagerImpl* operator->() const { return impl_; }
    // Small violation of C++ style guide to avoid polluting several
    // tests with get() calls.
    operator GpuDataManagerImpl*() { return impl_; }

   private:
    GpuDataManagerImpl* impl_;
    DISALLOW_COPY_AND_ASSIGN(ScopedGpuDataManagerImpl);
  };

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  base::Time JustBeforeExpiration(GpuDataManagerImpl* manager);
  base::Time JustAfterExpiration(GpuDataManagerImpl* manager);
  void TestBlockingDomainFrom3DAPIs(
      GpuDataManagerImpl::DomainGuilt guilt_level);
  void TestUnblockingDomainFrom3DAPIs(
      GpuDataManagerImpl::DomainGuilt guilt_level);

  MessageLoop message_loop_;
};

// We use new method instead of GetInstance() method because we want
// each test to be independent of each other.

TEST_F(GpuDataManagerImplTest, GpuSideBlacklisting) {
  // If a feature is allowed in preliminary step (browser side), but
  // disabled when GPU process launches and collects full GPU info,
  // it's too late to let renderer know, so we basically block all GPU
  // access, to be on the safe side.
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  const std::string blacklist_json = LONG_STRING_CONST(
      {
        "name": "gpu blacklist",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "features": [
              "webgl"
            ]
          },
          {
            "id": 2,
            "gl_renderer": {
              "op": "contains",
              "value": "GeForce"
            },
            "features": [
              "accelerated_2d_canvas"
            ]
          }
        ]
      }
  );

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  manager->InitializeForTesting(blacklist_json, gpu_info);

  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, manager->GetBlacklistedFeatures());

  gpu_info.gl_vendor = "NVIDIA";
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120";
  manager->UpdateGpuInfo(gpu_info);
  EXPECT_FALSE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL |
            GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
            manager->GetBlacklistedFeatures());
}

TEST_F(GpuDataManagerImplTest, GpuSideExceptions) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  const std::string blacklist_json = LONG_STRING_CONST(
      {
        "name": "gpu blacklist",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "exceptions": [
              {
                "gl_renderer": {
                  "op": "contains",
                  "value": "GeForce"
                }
              }
            ],
            "features": [
              "webgl"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  manager->InitializeForTesting(blacklist_json, gpu_info);

  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());

  // Now assume gpu process launches and full GPU info is collected.
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120";
  manager->UpdateGpuInfo(gpu_info);
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
}

TEST_F(GpuDataManagerImplTest, DisableHardwareAcceleration) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  manager->DisableHardwareAcceleration();
  EXPECT_FALSE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_ALL, manager->GetBlacklistedFeatures());
}

TEST_F(GpuDataManagerImplTest, SoftwareRendering) {
  // Blacklist, then register SwiftShader.
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_FALSE(manager->ShouldUseSoftwareRendering());

  manager->DisableHardwareAcceleration();
  EXPECT_FALSE(manager->GpuAccessAllowed());
  EXPECT_FALSE(manager->ShouldUseSoftwareRendering());

  // If software rendering is enabled, even if we blacklist GPU,
  // GPU process is still allowed.
  const base::FilePath test_path(FILE_PATH_LITERAL("AnyPath"));
  manager->RegisterSwiftShaderPath(test_path);
  EXPECT_TRUE(manager->ShouldUseSoftwareRendering());
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
            manager->GetBlacklistedFeatures());
}

TEST_F(GpuDataManagerImplTest, SoftwareRendering2) {
  // Register SwiftShader, then blacklist.
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_FALSE(manager->ShouldUseSoftwareRendering());

  const base::FilePath test_path(FILE_PATH_LITERAL("AnyPath"));
  manager->RegisterSwiftShaderPath(test_path);
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_FALSE(manager->ShouldUseSoftwareRendering());

  manager->DisableHardwareAcceleration();
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_TRUE(manager->ShouldUseSoftwareRendering());
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
            manager->GetBlacklistedFeatures());
}

TEST_F(GpuDataManagerImplTest, GpuInfoUpdate) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  TestObserver observer;
  manager->AddObserver(&observer);

  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.gpu_info_updated());

  GPUInfo gpu_info;
  manager->UpdateGpuInfo(gpu_info);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.gpu_info_updated());
}

TEST_F(GpuDataManagerImplTest, NoGpuInfoUpdateWithSoftwareRendering) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  manager->DisableHardwareAcceleration();
  const base::FilePath test_path(FILE_PATH_LITERAL("AnyPath"));
  manager->RegisterSwiftShaderPath(test_path);
  EXPECT_TRUE(manager->ShouldUseSoftwareRendering());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  TestObserver observer;
  manager->AddObserver(&observer);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.gpu_info_updated());

  GPUInfo gpu_info;
  manager->UpdateGpuInfo(gpu_info);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.gpu_info_updated());
}

TEST_F(GpuDataManagerImplTest, GPUVideoMemoryUsageStatsUpdate) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  TestObserver observer;
  manager->AddObserver(&observer);

  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.video_memory_usage_stats_updated());

  GPUVideoMemoryUsageStats vram_stats;
  manager->UpdateVideoMemoryUsageStats(vram_stats);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.video_memory_usage_stats_updated());
}

base::Time GpuDataManagerImplTest::JustBeforeExpiration(
    GpuDataManagerImpl* manager) {
  return GetTimeForTesting() + base::TimeDelta::FromMilliseconds(
      manager->GetBlockAllDomainsDurationInMs()) -
      base::TimeDelta::FromMilliseconds(3);
}

base::Time GpuDataManagerImplTest::JustAfterExpiration(
    GpuDataManagerImpl* manager) {
  return GetTimeForTesting() + base::TimeDelta::FromMilliseconds(
      manager->GetBlockAllDomainsDurationInMs()) +
      base::TimeDelta::FromMilliseconds(3);
}

void GpuDataManagerImplTest::TestBlockingDomainFrom3DAPIs(
    GpuDataManagerImpl::DomainGuilt guilt_level) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       guilt_level,
                                       GetTimeForTesting());

  // This domain should be blocked no matter what.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            JustBeforeExpiration(manager)));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            JustAfterExpiration(manager)));
}

void GpuDataManagerImplTest::TestUnblockingDomainFrom3DAPIs(
    GpuDataManagerImpl::DomainGuilt guilt_level) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       guilt_level,
                                       GetTimeForTesting());

  // Unblocking the domain should work.
  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            JustBeforeExpiration(manager)));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            JustAfterExpiration(manager)));
}

TEST_F(GpuDataManagerImplTest, BlockGuiltyDomainFrom3DAPIs) {
  TestBlockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_KNOWN);
}

TEST_F(GpuDataManagerImplTest, BlockDomainOfUnknownGuiltFrom3DAPIs) {
  TestBlockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
}

TEST_F(GpuDataManagerImplTest, BlockAllDomainsFrom3DAPIs) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN,
                                       GetTimeForTesting());

  // Blocking of other domains should expire.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            JustBeforeExpiration(manager)));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            JustAfterExpiration(manager)));
}

TEST_F(GpuDataManagerImplTest, UnblockGuiltyDomainFrom3DAPIs) {
  TestUnblockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_KNOWN);
}

TEST_F(GpuDataManagerImplTest, UnblockDomainOfUnknownGuiltFrom3DAPIs) {
  TestUnblockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
}

TEST_F(GpuDataManagerImplTest, UnblockOtherDomainFrom3DAPIs) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN,
                                       GetTimeForTesting());

  manager->UnblockDomainFrom3DAPIs(GetDomain2ForTesting());

  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            JustBeforeExpiration(manager)));

  // The original domain should still be blocked.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            JustBeforeExpiration(manager)));
}

TEST_F(GpuDataManagerImplTest, UnblockThisDomainFrom3DAPIs) {
  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN,
                                       GetTimeForTesting());

  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());

  // This behavior is debatable. Perhaps the GPU reset caused by
  // domain 1 should still cause other domains to be blocked.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            JustBeforeExpiration(manager)));
}

#if defined(OS_LINUX)
TEST_F(GpuDataManagerImplTest, SetGLStrings) {
  const char* kGLVendorMesa = "Tungsten Graphics, Inc";
  const char* kGLRendererMesa = "Mesa DRI Intel(R) G41";
  const char* kGLVersionMesa801 = "2.1 Mesa 8.0.1-DEVEL";

  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  const std::string blacklist_json = LONG_STRING_CONST(
      {
        "name": "gpu blacklist",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "vendor_id": "0x8086",
            "exceptions": [
              {
                "device_id": ["0x0042"],
                "driver_version": {
                  "op": ">=",
                  "number": "8.0.2"
                }
              }
            ],
            "features": [
              "webgl"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0042;
  manager->InitializeForTesting(blacklist_json, gpu_info);

  // Not enough GPUInfo.
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());

  // Now assume browser gets GL strings from local state.
  // The entry applies, blacklist more features than from the preliminary step.
  // However, GPU process is not blocked because this is all browser side and
  // happens before renderer launching.
  manager->SetGLStrings(kGLVendorMesa, kGLRendererMesa, kGLVersionMesa801);
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, manager->GetBlacklistedFeatures());
}

TEST_F(GpuDataManagerImplTest, SetGLStringsNoEffects) {
  const char* kGLVendorMesa = "Tungsten Graphics, Inc";
  const char* kGLRendererMesa = "Mesa DRI Intel(R) G41";
  const char* kGLVersionMesa801 = "2.1 Mesa 8.0.1-DEVEL";
  const char* kGLVersionMesa802 = "2.1 Mesa 8.0.2-DEVEL";

  ScopedGpuDataManagerImpl manager;
  ASSERT_TRUE(manager.get());
  EXPECT_EQ(0, manager->GetBlacklistedFeatures());
  EXPECT_TRUE(manager->GpuAccessAllowed());

  const std::string blacklist_json = LONG_STRING_CONST(
      {
        "name": "gpu blacklist",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "vendor_id": "0x8086",
            "exceptions": [
              {
                "device_id": ["0x0042"],
                "driver_version": {
                  "op": ">=",
                  "number": "8.0.2"
                }
              }
            ],
            "features": [
              "webgl"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0042;
  gpu_info.gl_vendor = kGLVendorMesa;
  gpu_info.gl_renderer = kGLRendererMesa;
  gpu_info.gl_version = kGLVersionMesa801;
  gpu_info.driver_vendor = "Mesa";
  gpu_info.driver_version = "8.0.1";
  manager->InitializeForTesting(blacklist_json, gpu_info);

  // Full GPUInfo, the entry applies.
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, manager->GetBlacklistedFeatures());

  // Now assume browser gets GL strings from local state.
  // SetGLStrings() has no effects because GPUInfo already got these strings.
  // (Otherwise the entry should not apply.)
  manager->SetGLStrings(kGLVendorMesa, kGLRendererMesa, kGLVersionMesa802);
  EXPECT_TRUE(manager->GpuAccessAllowed());
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, manager->GetBlacklistedFeatures());
}
#endif  // OS_LINUX

}  // namespace content
