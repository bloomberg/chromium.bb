// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

class CrOSMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  CrOSMockComponentUpdateService() {}
  ~CrOSMockComponentUpdateService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CrOSMockComponentUpdateService);
};

class CrOSComponentInstallerTest : public PlatformTest {
 public:
  CrOSComponentInstallerTest() {}
  void SetUp() override { PlatformTest::SetUp(); }

 protected:
  void RunUntilIdle() {
    scoped_task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerTest);
};

class MockCrOSComponentInstallerTraits : public CrOSComponentInstallerTraits {
 public:
  explicit MockCrOSComponentInstallerTraits(const ComponentConfig& config)
      : CrOSComponentInstallerTraits(config) {}
  MOCK_METHOD2(IsCompatible,
               bool(const std::string& env_version_str,
                    const std::string& min_env_version_str));
};

void load_callback(const std::string& result) {}

TEST_F(CrOSComponentInstallerTest, BPPPCompatibleCrOSComponent) {
  BrowserProcessPlatformPart bppp;
  ASSERT_EQ(bppp.IsCompatibleCrOSComponent("a"), false);
  bppp.AddCompatibleCrOSComponent("a");
  ASSERT_EQ(bppp.IsCompatibleCrOSComponent("a"), true);
}

TEST_F(CrOSComponentInstallerTest, ComponentReadyCorrectManifest) {
  ComponentConfig config("a", "2.1", "");
  MockCrOSComponentInstallerTraits traits(config);
  EXPECT_CALL(traits, IsCompatible(testing::_, testing::_)).Times(1);
  base::Version version;
  base::FilePath path;
  std::unique_ptr<base::DictionaryValue> manifest =
      base::MakeUnique<base::DictionaryValue>();
  manifest->SetString("min_env_version", "2.1");
  traits.ComponentReady(version, path, std::move(manifest));
  RunUntilIdle();
}

TEST_F(CrOSComponentInstallerTest, ComponentReadyWrongManifest) {
  ComponentConfig config("a", "2.1", "");
  MockCrOSComponentInstallerTraits traits(config);
  EXPECT_CALL(traits, IsCompatible(testing::_, testing::_)).Times(0);
  base::Version version;
  base::FilePath path;
  std::unique_ptr<base::DictionaryValue> manifest =
      base::MakeUnique<base::DictionaryValue>();
  traits.ComponentReady(version, path, std::move(manifest));
  RunUntilIdle();
}

TEST_F(CrOSComponentInstallerTest, IsCompatibleOrNot) {
  ComponentConfig config("", "", "");
  CrOSComponentInstallerTraits traits(config);
  EXPECT_TRUE(traits.IsCompatible("1.0", "1.0"));
  EXPECT_TRUE(traits.IsCompatible("1.1", "1.0"));
  EXPECT_FALSE(traits.IsCompatible("1.0", "1.1"));
  EXPECT_FALSE(traits.IsCompatible("1.0", "2.0"));
  EXPECT_FALSE(traits.IsCompatible("1.c", "1.c"));
  EXPECT_FALSE(traits.IsCompatible("1", "1.1"));
  EXPECT_TRUE(traits.IsCompatible("1.1.1", "1.1"));
}

}  // namespace component_updater
