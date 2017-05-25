// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cros_component_installer.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
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
  DISALLOW_COPY_AND_ASSIGN(CrOSMockComponentUpdateService);
};

class CrOSComponentInstallerTest : public PlatformTest {
 public:
  CrOSComponentInstallerTest() {}
  void SetUp() override { PlatformTest::SetUp(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(CrOSComponentInstallerTest);
};

class FakeInstallerTraits : public ComponentInstallerTraits {
 public:
  ~FakeInstallerTraits() override {}

  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override {
    return true;
  }

  bool SupportsGroupPolicyEnabledComponentUpdates() const override {
    return true;
  }

  bool RequiresNetworkEncryption() const override { return true; }

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override {
    return update_client::CrxInstaller::Result(0);
  }

  void ComponentReady(
      const base::Version& version,
      const base::FilePath& install_dir,
      std::unique_ptr<base::DictionaryValue> manifest) override {}

  base::FilePath GetRelativeInstallDir() const override {
    return base::FilePath(FILE_PATH_LITERAL("fake"));
  }

  void GetHash(std::vector<uint8_t>* hash) const override {}

  std::string GetName() const override { return "fake name"; }

  update_client::InstallerAttributes GetInstallerAttributes() const override {
    update_client::InstallerAttributes installer_attributes;
    return installer_attributes;
  }

  std::vector<std::string> GetMimeTypes() const override {
    return std::vector<std::string>();
  }
};

void install_callback(update_client::Error error) {}

TEST_F(CrOSComponentInstallerTest, BPPPCompatibleCrOSComponent) {
  auto* bppp = new BrowserProcessPlatformPart();
  ASSERT_EQ(bppp->IsCompatibleCrOSComponent("a"), false);
  bppp->AddCompatibleCrOSComponent("a");
  ASSERT_EQ(bppp->IsCompatibleCrOSComponent("a"), true);
}

TEST_F(CrOSComponentInstallerTest, RegisterComponentFail) {
  std::unique_ptr<CrOSMockComponentUpdateService> cus =
      base::MakeUnique<CrOSMockComponentUpdateService>();
  EXPECT_CALL(*cus, RegisterComponent(testing::_)).Times(0);
  component_updater::CrOSComponent::InstallCrOSComponent(
      "a-component-not-exist", base::Bind(install_callback));
  base::RunLoop().RunUntilIdle();
}

}  // namespace component_updater
