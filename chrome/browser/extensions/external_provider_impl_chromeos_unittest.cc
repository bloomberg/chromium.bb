// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"

namespace extensions {

namespace {

const char kExternalAppId[] = "kekdneafjmhmndejhmbcadfiiofngffo";

class ExternalProviderImplTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplTest() {}
  virtual ~ExternalProviderImplTest() {}

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
  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();

    external_externsions_overrides_.reset(
        new base::ScopedPathOverride(chrome::DIR_EXTERNAL_EXTENSIONS,
                                     data_dir_.Append("external")));
  }

 private:
  scoped_ptr<base::ScopedPathOverride> external_externsions_overrides_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProviderImplTest);
};

}  // namespace

// Normal mode, external app should be installed.
TEST_F(ExternalProviderImplTest, Normal) {
  InitServiceWithExternalProviders();

  service_->CheckForExternalUpdates();
  loop_.RunUntilIdle();

  EXPECT_TRUE(service_->GetInstalledExtension(kExternalAppId));
}

// App mode, no external app should be installed.
TEST_F(ExternalProviderImplTest, AppMode) {
  CommandLine* command = CommandLine::ForCurrentProcess();
  command->AppendSwitchASCII(switches::kForceAppMode, std::string());
  command->AppendSwitchASCII(switches::kAppId, std::string("app_id"));

  InitServiceWithExternalProviders();

  service_->CheckForExternalUpdates();
  loop_.RunUntilIdle();

  EXPECT_FALSE(service_->GetInstalledExtension(kExternalAppId));
}

}  // namespace extensions
