// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/fake_user_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

namespace extensions {

namespace {

const char kExternalAppId[] = "kekdneafjmhmndejhmbcadfiiofngffo";

class ExternalProviderImplChromeOSTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplChromeOSTest()
      : fake_user_manager_(new user_manager::FakeUserManager()),
        scoped_user_manager_(fake_user_manager_) {}

  ~ExternalProviderImplChromeOSTest() override {}

  void InitServiceWithExternalProviders() {
    InitializeEmptyExtensionService();
    service_->Init();

    ProviderCollection providers;
    extensions::ExternalProviderImpl::CreateExternalProviders(
        service_, profile_.get(), &providers);

    for (ProviderCollection::iterator i = providers.begin();
         i != providers.end();
         ++i) {
      service_->AddProviderForTesting(i->release());
    }
  }

  // ExtensionServiceTestBase overrides:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    external_externsions_overrides_.reset(new base::ScopedPathOverride(
        chrome::DIR_EXTERNAL_EXTENSIONS, data_dir().Append("external")));
  }

  void TearDown() override {
    chromeos::KioskAppManager::Shutdown();
  }

 private:
  scoped_ptr<base::ScopedPathOverride> external_externsions_overrides_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  user_manager::FakeUserManager* fake_user_manager_;
  chromeos::ScopedUserManagerEnabler scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProviderImplChromeOSTest);
};

}  // namespace

// Normal mode, external app should be installed.
TEST_F(ExternalProviderImplChromeOSTest, Normal) {
  InitServiceWithExternalProviders();

  service_->CheckForExternalUpdates();
  content::WindowedNotificationObserver(
      extensions::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources()).Wait();

  EXPECT_TRUE(service_->GetInstalledExtension(kExternalAppId));
}

// App mode, no external app should be installed.
TEST_F(ExternalProviderImplChromeOSTest, AppMode) {
  base::CommandLine* command = base::CommandLine::ForCurrentProcess();
  command->AppendSwitchASCII(switches::kForceAppMode, std::string());
  command->AppendSwitchASCII(switches::kAppId, std::string("app_id"));

  InitServiceWithExternalProviders();

  service_->CheckForExternalUpdates();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(service_->GetInstalledExtension(kExternalAppId));
}

}  // namespace extensions
