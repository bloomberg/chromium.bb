// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "content/browser/gpu/gpu_data_manager_testing_autogen.h"
#include "content/browser/gpu/gpu_data_manager_testing_entry_enums_autogen.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace content {
namespace {

class TestObserver : public GpuDataManagerObserver {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  bool gpu_info_updated() const { return gpu_info_updated_; }

  void OnGpuInfoUpdate() override { gpu_info_updated_ = true; }

  void Reset() {
    gpu_info_updated_ = false;
  }

 private:
  bool gpu_info_updated_ = false;
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

class GpuDataManagerImplPrivateTest : public testing::Test {
 public:
  GpuDataManagerImplPrivateTest() {}
  ~GpuDataManagerImplPrivateTest() override {}

 protected:
  // scoped_ptr doesn't work with GpuDataManagerImpl because its
  // destructor is private. GpuDataManagerImplPrivateTest is however a friend
  // so we can make a little helper class here.
  class ScopedGpuDataManagerImpl {
   public:
    ScopedGpuDataManagerImpl() : impl_(new GpuDataManagerImpl()) {
      EXPECT_TRUE(impl_);
      EXPECT_TRUE(impl_->private_.get());
    }
    ~ScopedGpuDataManagerImpl() { delete impl_; }

    GpuDataManagerImpl* get() const { return impl_; }

    GpuDataManagerImpl* operator->() const { return impl_; }

   private:
    GpuDataManagerImpl* impl_;
    DISALLOW_COPY_AND_ASSIGN(ScopedGpuDataManagerImpl);
  };

  // We want to test the code path where GpuDataManagerImplPrivate is created
  // in the GpuDataManagerImpl constructor.
  class ScopedGpuDataManagerImplPrivate {
   public:
    ScopedGpuDataManagerImplPrivate() : impl_(new GpuDataManagerImpl()) {
      EXPECT_TRUE(impl_);
      EXPECT_TRUE(impl_->private_.get());
    }
    ~ScopedGpuDataManagerImplPrivate() { delete impl_; }

    GpuDataManagerImplPrivate* get() const {
      return impl_->private_.get();
    }

    GpuDataManagerImplPrivate* operator->() const {
      return impl_->private_.get();
    }

   private:
    GpuDataManagerImpl* impl_;
    DISALLOW_COPY_AND_ASSIGN(ScopedGpuDataManagerImplPrivate);
  };

  void SetUp() override {}

  void TearDown() override {}

  base::Time JustBeforeExpiration(const GpuDataManagerImplPrivate* manager);
  base::Time JustAfterExpiration(const GpuDataManagerImplPrivate* manager);
  void TestBlockingDomainFrom3DAPIs(
      GpuDataManagerImpl::DomainGuilt guilt_level);
  void TestUnblockingDomainFrom3DAPIs(
      GpuDataManagerImpl::DomainGuilt guilt_level);

  base::MessageLoop message_loop_;
};

// We use new method instead of GetInstance() method because we want
// each test to be independent of each other.

TEST_F(GpuDataManagerImplPrivateTest, GpuSideBlacklisting) {
  // If a feature is allowed in preliminary step (browser side), but
  // disabled when GPU process launches and collects full GPU info,
  // it's too late to let renderer know, so we basically block all GPU
  // access, to be on the safe side.
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  std::string reason;
  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_GpuSideBlacklisting_0],
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_GpuSideBlacklisting_1],
  };
  const gpu::GpuControlListData kData("1.0", 2, kEntries);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  manager->InitializeForTesting(kData, gpu_info);

  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }
  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));

  gpu_info.gl_vendor = "NVIDIA";
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120";
  manager->UpdateGpuInfo(gpu_info);
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
    EXPECT_TRUE(reason.empty());
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
    EXPECT_TRUE(manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL2));
  } else {
    EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
    EXPECT_TRUE(reason.empty());
    EXPECT_EQ(2u, manager->GetBlacklistedFeatureCount());
    EXPECT_FALSE(manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL2));
  }
  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));
  EXPECT_TRUE(manager->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));
}

TEST_F(GpuDataManagerImplPrivateTest, GpuSideBlacklistingWebGL) {
  // If a feature is allowed in preliminary step (browser side), but
  // disabled when GPU process launches and collects full GPU info,
  // it's too late to let renderer know, so we basically block all GPU
  // access, to be on the safe side.
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  std::string reason;
  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_GpuSideBlacklistingWebGL_0],
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_GpuSideBlacklistingWebGL_1],
  };
  const gpu::GpuControlListData kData("1.0", 2, kEntries);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  manager->InitializeForTesting(kData, gpu_info);

  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }
  EXPECT_TRUE(manager->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));

  gpu_info.gl_vendor = "NVIDIA";
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120";
  manager->UpdateGpuInfo(gpu_info);
  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(3u, manager->GetBlacklistedFeatureCount());
  }
  EXPECT_TRUE(manager->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));
  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));
  EXPECT_TRUE(manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL2));
}

TEST_F(GpuDataManagerImplPrivateTest, GpuSideExceptions) {
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_GpuSideException],
  };
  const gpu::GpuControlListData kData("1.0", 1, kEntries);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  manager->InitializeForTesting(kData, gpu_info);

  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  EXPECT_EQ(manager->ShouldUseSwiftShader()
                ? static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES)
                : 1u,
            manager->GetBlacklistedFeatureCount());

  // Now assume gpu process launches and full GPU info is collected.
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120";
  manager->UpdateGpuInfo(gpu_info);
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  // Since SwiftShader was enabled by first gpu_info,  UpdateGpuInfo
  // should have failed and SwiftShader should still be active
  EXPECT_EQ(manager->ShouldUseSwiftShader()
                ? static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES)
                : 0u,
            manager->GetBlacklistedFeatureCount());
}

TEST_F(GpuDataManagerImplPrivateTest, DisableHardwareAcceleration) {
  ScopedGpuDataManagerImplPrivate manager;
  const gpu::GpuControlListData kData;
  manager->InitializeForTesting(kData, gpu::GPUInfo());
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  std::string reason;
  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());

  manager->DisableHardwareAcceleration();
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
    EXPECT_TRUE(reason.empty());
  } else {
    EXPECT_FALSE(manager->GpuAccessAllowed(&reason));
    EXPECT_FALSE(reason.empty());
  }
  EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
            manager->GetBlacklistedFeatureCount());
}

TEST_F(GpuDataManagerImplPrivateTest, SwiftShaderRendering) {
  // Blacklist, then register SwiftShader.
  ScopedGpuDataManagerImplPrivate manager;
  const gpu::GpuControlListData kData;
  manager->InitializeForTesting(kData, gpu::GPUInfo());
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  EXPECT_FALSE(manager->ShouldUseSwiftShader());

  manager->DisableHardwareAcceleration();
  EXPECT_EQ(manager->ShouldUseSwiftShader(), manager->GpuAccessAllowed(NULL));
  EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
            manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));
  EXPECT_TRUE(manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL2));
}

TEST_F(GpuDataManagerImplPrivateTest, SwiftShaderRendering2) {
  // Register SwiftShader, then blacklist.
  ScopedGpuDataManagerImplPrivate manager;
  const gpu::GpuControlListData kData;
  manager->InitializeForTesting(kData, gpu::GPUInfo());
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  EXPECT_FALSE(manager->ShouldUseSwiftShader());

  manager->DisableHardwareAcceleration();
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  } else {
    EXPECT_FALSE(manager->GpuAccessAllowed(NULL));
  }
  EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
            manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->IsFeatureBlacklisted(
      gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS));
  EXPECT_TRUE(manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL2));
}

TEST_F(GpuDataManagerImplPrivateTest, GpuInfoUpdate) {
  ScopedGpuDataManagerImpl manager;

  TestObserver observer;
  manager->AddObserver(&observer);

  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.gpu_info_updated());

  gpu::GPUInfo gpu_info;
  manager->UpdateGpuInfo(gpu_info);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.gpu_info_updated());
}

TEST_F(GpuDataManagerImplPrivateTest, NoGpuInfoUpdateWithSwiftShader) {
  ScopedGpuDataManagerImpl manager;
  const gpu::GpuControlListData kData;
  manager->InitializeForTesting(kData, gpu::GPUInfo());

  manager->DisableHardwareAcceleration();
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  } else {
    EXPECT_FALSE(manager->GpuAccessAllowed(NULL));
  }

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

  gpu::GPUInfo gpu_info;
  manager->UpdateGpuInfo(gpu_info);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  if (manager->ShouldUseSwiftShader()) {
    // Once SwiftShader is enabled, the gpu info can no longer be updated
    EXPECT_FALSE(observer.gpu_info_updated());
  } else {
    EXPECT_TRUE(observer.gpu_info_updated());
  }
}

base::Time GpuDataManagerImplPrivateTest::JustBeforeExpiration(
    const GpuDataManagerImplPrivate* manager) {
  return GetTimeForTesting() + base::TimeDelta::FromMilliseconds(
      manager->GetBlockAllDomainsDurationInMs()) -
      base::TimeDelta::FromMilliseconds(3);
}

base::Time GpuDataManagerImplPrivateTest::JustAfterExpiration(
    const GpuDataManagerImplPrivate* manager) {
  return GetTimeForTesting() + base::TimeDelta::FromMilliseconds(
      manager->GetBlockAllDomainsDurationInMs()) +
      base::TimeDelta::FromMilliseconds(3);
}

void GpuDataManagerImplPrivateTest::TestBlockingDomainFrom3DAPIs(
    GpuDataManagerImpl::DomainGuilt guilt_level) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                      guilt_level,
                                      GetTimeForTesting());

  // This domain should be blocked no matter what.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                           GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustAfterExpiration(manager.get())));
}

void GpuDataManagerImplPrivateTest::TestUnblockingDomainFrom3DAPIs(
    GpuDataManagerImpl::DomainGuilt guilt_level) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       guilt_level,
                                       GetTimeForTesting());

  // Unblocking the domain should work.
  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustAfterExpiration(manager.get())));
}

TEST_F(GpuDataManagerImplPrivateTest, BlockGuiltyDomainFrom3DAPIs) {
  TestBlockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_KNOWN);
}

TEST_F(GpuDataManagerImplPrivateTest, BlockDomainOfUnknownGuiltFrom3DAPIs) {
  TestBlockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
}

TEST_F(GpuDataManagerImplPrivateTest, BlockAllDomainsFrom3DAPIs) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN,
                                       GetTimeForTesting());

  // Blocking of other domains should expire.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustAfterExpiration(manager.get())));
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockGuiltyDomainFrom3DAPIs) {
  TestUnblockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_KNOWN);
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockDomainOfUnknownGuiltFrom3DAPIs) {
  TestUnblockingDomainFrom3DAPIs(GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN);
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockOtherDomainFrom3DAPIs) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN,
                                       GetTimeForTesting());

  manager->UnblockDomainFrom3DAPIs(GetDomain2ForTesting());

  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));

  // The original domain should still be blocked.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockThisDomainFrom3DAPIs) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       GpuDataManagerImpl::DOMAIN_GUILT_UNKNOWN,
                                       GetTimeForTesting());

  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());

  // This behavior is debatable. Perhaps the GPU reset caused by
  // domain 1 should still cause other domains to be blocked.
  EXPECT_EQ(GpuDataManagerImpl::DOMAIN_BLOCK_STATUS_NOT_BLOCKED,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));
}

#if defined(OS_LINUX)
TEST_F(GpuDataManagerImplPrivateTest, SetGLStrings) {
  const char* kGLVendorMesa = "Tungsten Graphics, Inc";
  const char* kGLRendererMesa = "Mesa DRI Intel(R) G41";
  const char* kGLVersionMesa801 = "2.1 Mesa 8.0.1-DEVEL";

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_SetGLStrings],
  };
  const gpu::GpuControlListData kData("1.0", 1, kEntries);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0042;
  manager->InitializeForTesting(kData, gpu_info);

  // Not enough GPUInfo.
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }

  // Now assume browser gets GL strings from local state.
  // The entry applies, blacklist more features than from the preliminary step.
  // However, GPU process is not blocked because this is all browser side and
  // happens before renderer launching.
  manager->SetGLStrings(kGLVendorMesa, kGLRendererMesa, kGLVersionMesa801);
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }
  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));
}

TEST_F(GpuDataManagerImplPrivateTest, SetGLStringsNoEffects) {
  const char* kGLVendorMesa = "Tungsten Graphics, Inc";
  const char* kGLRendererMesa = "Mesa DRI Intel(R) G41";
  const char* kGLVersionMesa801 = "2.1 Mesa 8.0.1-DEVEL";
  const char* kGLVersionMesa802 = "2.1 Mesa 8.0.2-DEVEL";

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_SetGLStringsNoEffects],
  };
  const gpu::GpuControlListData kData("1.0", 1, kEntries);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0042;
  gpu_info.gl_vendor = kGLVendorMesa;
  gpu_info.gl_renderer = kGLRendererMesa;
  gpu_info.gl_version = kGLVersionMesa801;
  gpu_info.driver_vendor = "Mesa";
  gpu_info.driver_version = "8.0.1";
  manager->InitializeForTesting(kData, gpu_info);

  // Full GPUInfo, the entry applies.
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }
  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));

  // Now assume browser gets GL strings from local state.
  // SetGLStrings() has no effects because GPUInfo already got these strings.
  // (Otherwise the entry should not apply.)
  manager->SetGLStrings(kGLVendorMesa, kGLRendererMesa, kGLVersionMesa802);
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }
  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));
}

TEST_F(GpuDataManagerImplPrivateTest, SetGLStringsDefered) {
  const char* kGLVendorMesa = "Tungsten Graphics, Inc";
  const char* kGLRendererMesa = "Mesa DRI Intel(R) G41";
  const char* kGLVersionMesa801 = "2.1 Mesa 8.0.1-DEVEL";

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_SetGLStringsDefered],
  };
  const gpu::GpuControlListData kData("1.0", 1, kEntries);

  // Check that it is allowed to call SetGLStrings before Initialize.

  // Assume browser gets GL strings from local state.
  manager->SetGLStrings(kGLVendorMesa, kGLRendererMesa, kGLVersionMesa801);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0042;
  manager->InitializeForTesting(kData, gpu_info);

  EXPECT_TRUE(manager->GpuAccessAllowed(NULL));
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }

  EXPECT_TRUE(
      manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_ACCELERATED_WEBGL));
}
#endif  // OS_LINUX

TEST_F(GpuDataManagerImplPrivateTest, BlacklistAllFeatures) {
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  std::string reason;
  EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
  EXPECT_TRUE(reason.empty());

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_BlacklistAllFeatures],
  };
  const gpu::GpuControlListData kData("1.0", 1, kEntries);

  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  manager->InitializeForTesting(kData, gpu_info);

  EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
            manager->GetBlacklistedFeatureCount());
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_TRUE(manager->GpuAccessAllowed(&reason));
    EXPECT_TRUE(reason.empty());
  }
}

TEST_F(GpuDataManagerImplPrivateTest, UpdateActiveGpu) {
  ScopedGpuDataManagerImpl manager;

  const gpu::GpuControlList::Entry kEntries[] = {
      gpu::kGpuDataManagerTestingEntries
          [gpu::kGpuDataManagerImplPrivateTest_UpdateActiveGpu],
  };
  const gpu::GpuControlListData kData("1.0", 1, kEntries);

  // Two GPUs, the secondary Intel GPU is active.
  gpu::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  gpu_info.gpu.active = false;
  gpu::GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x04a1;
  intel_gpu.active = true;
  gpu_info.secondary_gpus.push_back(intel_gpu);

  manager->InitializeForTesting(kData, gpu_info);
  TestObserver observer;
  manager->AddObserver(&observer);

  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());

    // Update to previous Intel GPU.
    EXPECT_TRUE(manager->UpdateActiveGpu(0x8086, 0x04a1));
    {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    EXPECT_TRUE(observer.gpu_info_updated());
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());

    // Update to previous Intel GPU.
    EXPECT_FALSE(manager->UpdateActiveGpu(0x8086, 0x04a1));
    {
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
    EXPECT_FALSE(observer.gpu_info_updated());
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }

  // Set NVIDIA GPU to be active.
  EXPECT_TRUE(manager->UpdateActiveGpu(0x10de, 0x0640));
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.gpu_info_updated());
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  }

  observer.Reset();
  EXPECT_FALSE(observer.gpu_info_updated());

  // Update to previous NVIDIA GPU.
  EXPECT_FALSE(manager->UpdateActiveGpu(0x10de, 0x0640));
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.gpu_info_updated());

  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(0u, manager->GetBlacklistedFeatureCount());
  }

  // Set Intel GPU to be active.
  EXPECT_TRUE(manager->UpdateActiveGpu(0x8086, 0x04a1));
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.gpu_info_updated());
  if (manager->ShouldUseSwiftShader()) {
    EXPECT_EQ(static_cast<size_t>(gpu::NUMBER_OF_GPU_FEATURE_TYPES),
              manager->GetBlacklistedFeatureCount());
  } else {
    EXPECT_EQ(1u, manager->GetBlacklistedFeatureCount());
  }
}

}  // namespace content
