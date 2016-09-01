// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/zipfile_installer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

namespace {

struct MockExtensionRegistryObserver : public ExtensionRegistryObserver {
  void WaitForInstall() {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    quit_closure_ = runner->QuitClosure();
    runner->Run();
  }

  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    last_extension_installed = extension->id();
    quit_closure_.Run();
  }

  std::string last_extension_installed;
  base::Closure quit_closure_;
};

}  // namespace

class ZipFileInstallerTest : public testing::Test {
 public:
  ZipFileInstallerTest()
      : browser_threads_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    in_process_utility_thread_helper_.reset(
        new content::InProcessUtilityThreadHelper);

    // Create profile for extension service.
    profile_.reset(new TestingProfile());
    TestExtensionSystem* system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
    extension_service_ = system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ExtensionRegistry* registry(ExtensionRegistry::Get(profile_.get()));
    registry->AddObserver(&observer_);
  }

  void TearDown() override {
    // Need to destruct ZipFileInstaller before the message loop since
    // it posts a task to it.
    zipfile_installer_ = NULL;
    ExtensionRegistry* registry(ExtensionRegistry::Get(profile_.get()));
    registry->RemoveObserver(&observer_);
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void RunInstaller(const std::string& zip_name) {
    base::FilePath original_path;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &original_path));
    original_path = original_path.AppendASCII("extensions")
                        .AppendASCII("zipfile_installer")
                        .AppendASCII(zip_name);
    ASSERT_TRUE(base::PathExists(original_path)) << original_path.value();

    zipfile_installer_ = ZipFileInstaller::Create(extension_service_);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ZipFileInstaller::LoadFromZipFile,
                   zipfile_installer_,
                   original_path));
    observer_.WaitForInstall();
  }

 protected:
  scoped_refptr<ZipFileInstaller> zipfile_installer_;

  std::unique_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;

  content::TestBrowserThreadBundle browser_threads_;
  std::unique_ptr<content::InProcessUtilityThreadHelper>
      in_process_utility_thread_helper_;
  MockExtensionRegistryObserver observer_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  // ChromeOS needs a user manager to instantiate an extension service.
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ZipFileInstallerTest, GoodZip) {
  RunInstaller("good.zip");
}

TEST_F(ZipFileInstallerTest, ZipWithPublicKey) {
  RunInstaller("public_key.zip");
  const char kIdForPublicKey[] = "ikppjpenhoddphklkpdfdfdabbakkpal";
  EXPECT_EQ(observer_.last_extension_installed, kIdForPublicKey);
}

}  // namespace extensions
