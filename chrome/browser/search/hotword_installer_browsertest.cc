// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MockHotwordWebstoreInstaller
    : public HotwordService::HotwordWebstoreInstaller {
 public:
  MockHotwordWebstoreInstaller(Profile* profile, const Callback& callback)
      : HotwordService::HotwordWebstoreInstaller(
            extension_misc::kHotwordSharedModuleId,
            profile,
            callback) {
  }

  const GURL& GetRequestorURL() const override {
    // This should not be valid so it hangs.
    return GURL::EmptyGURL();
  }

  void BeginInstall() {
    WebstoreStandaloneInstaller::BeginInstall();
  }

 private:
  ~MockHotwordWebstoreInstaller() override {}
};


class MockHotwordService : public HotwordService {
 public:
  explicit MockHotwordService(Profile* profile)
      : HotwordService(profile),
        profile_(profile),
        weak_factory_(this) {
  }

  void InstallHotwordExtensionFromWebstore(int num_tries) override {
    installer_ = new MockHotwordWebstoreInstaller(
        profile_,
        base::Bind(&MockHotwordService::InstallerCallback,
                   weak_factory_.GetWeakPtr(),
                   num_tries - 1));
    installer_->BeginInstall();
  }

  void InstallerCallback(int num_tries,
                         bool success,
                         const std::string& error,
                         extensions::webstore_install::Result result) {
  }

 private:
  Profile* profile_;
  base::WeakPtrFactory<MockHotwordService> weak_factory_;
};

std::unique_ptr<KeyedService> BuildMockHotwordService(
    content::BrowserContext* context) {
  return base::MakeUnique<MockHotwordService>(static_cast<Profile*>(context));
}

}  // namespace

namespace extensions {

class HotwordInstallerBrowserTest : public ExtensionBrowserTest {
 public:
  HotwordInstallerBrowserTest() {}
  ~HotwordInstallerBrowserTest() override {}

 private:
   DISALLOW_COPY_AND_ASSIGN(HotwordInstallerBrowserTest);
};

// Test that installing to a non-existent URL (which should hang) does not
// crash. This test is successful if it does not crash and trigger any DCHECKS.
IN_PROC_BROWSER_TEST_F(HotwordInstallerBrowserTest, AbortInstallOnShutdown) {
  TestingProfile test_profile;
  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();
  MockHotwordService* hotword_service = static_cast<MockHotwordService*>(
      hotword_service_factory->SetTestingFactoryAndUse(
          &test_profile, BuildMockHotwordService));
  hotword_service->InstallHotwordExtensionFromWebstore(1);
}

}  // namespace extensions
