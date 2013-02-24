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
#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/common/extensions/feature_switch.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingProfile;

namespace extensions {
class ManagementPolicy;
}

class ExtensionServiceTestBase : public testing::Test {
 public:
  ExtensionServiceTestBase();
  virtual ~ExtensionServiceTestBase();

  void InitializeExtensionService(const base::FilePath& profile_path,
                                  const base::FilePath& pref_file,
                                  const base::FilePath& extensions_install_dir,
                                  bool autoupdate_enabled);

  void InitializeInstalledExtensionService(
      const base::FilePath& prefs_file,
      const base::FilePath& source_install_dir);

  void InitializeEmptyExtensionService();

  void InitializeExtensionProcessManager();

  void InitializeExtensionServiceWithUpdater();

  void InitializeRequestContext();

  static void SetUpTestCase();

  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

  void set_extensions_enabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

 protected:
  void InitializeExtensionServiceHelper(bool autoupdate_enabled);

  MessageLoop loop_;
  base::ShadowingAtExitManager at_exit_manager_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  base::FilePath extensions_install_dir_;
  base::FilePath data_dir_;
  // Managed by extensions::ExtensionSystemFactory.
  ExtensionService* service_;
  extensions::ManagementPolicy* management_policy_;
  size_t expected_extensions_count_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread webkit_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_user_blocking_thread_;
  content::TestBrowserThread io_thread_;
  extensions::FeatureSwitch::ScopedOverride override_sideload_wipeout_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
