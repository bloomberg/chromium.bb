// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

class CrOSMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  CrOSMockComponentUpdateService() {}
  ~CrOSMockComponentUpdateService() override {}

  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrOSMockComponentUpdateService);
};

class CrOSComponentInstallerTest : public PlatformTest {
 public:
  CrOSComponentInstallerTest() {}

  void SetUp() override { PlatformTest::SetUp(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerTest);
};

#if defined(OS_CHROMEOS)
TEST_F(CrOSComponentInstallerTest, RegisterESCPR) {
  std::unique_ptr<CrOSMockComponentUpdateService> cus =
      base::MakeUnique<CrOSMockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(1);
  component_updater::RegisterCrOSComponent(cus.get(), "escpr");
  base::RunLoop().RunUntilIdle();
}
TEST_F(CrOSComponentInstallerTest, RegisterFakeComponent) {
  std::unique_ptr<CrOSMockComponentUpdateService> cus =
      base::MakeUnique<CrOSMockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);
  RegisterCrOSComponent(cus.get(), "arandomname");
  base::RunLoop().RunUntilIdle();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace component_updater
