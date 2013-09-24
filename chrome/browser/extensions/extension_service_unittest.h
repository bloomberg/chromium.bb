// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/feature_switch.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

class TestingProfile;

namespace extensions {
class ManagementPolicy;
}

class ExtensionServiceTestBase : public testing::Test {
 public:
  struct ExtensionServiceInitParams {
    base::FilePath profile_path;
    base::FilePath pref_file;
    base::FilePath extensions_install_dir;
    bool autoupdate_enabled;
    bool is_first_run;

    ExtensionServiceInitParams();
  };

  ExtensionServiceTestBase();
  virtual ~ExtensionServiceTestBase();

  void InitializeExtensionService(const ExtensionServiceInitParams& params);

  void InitializeInstalledExtensionService(
      const base::FilePath& prefs_file,
      const base::FilePath& source_install_dir);

  void InitializeGoodInstalledExtensionService();

  void InitializeEmptyExtensionService();

  void InitializeExtensionProcessManager();

  void InitializeExtensionServiceWithUpdater();

  static void SetUpTestCase();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void set_extensions_enabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

 protected:
  void InitializeExtensionServiceHelper(bool autoupdate_enabled,
                                        bool is_first_run);

  // Destroying at_exit_manager_ will delete all LazyInstances, so it must come
  // after thread_bundle_ in the destruction order.
  base::ShadowingAtExitManager at_exit_manager_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  base::FilePath extensions_install_dir_;
  base::FilePath data_dir_;
  // Managed by extensions::ExtensionSystemFactory.
  ExtensionService* service_;
  extensions::ManagementPolicy* management_policy_;
  size_t expected_extensions_count_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
