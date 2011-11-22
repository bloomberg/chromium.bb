// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
#pragma once

#include "base/at_exit.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestingProfile;

class ExtensionServiceTestBase : public testing::Test {
 public:
  ExtensionServiceTestBase();
  virtual ~ExtensionServiceTestBase();

  void InitializeExtensionService(
      const FilePath& pref_file, const FilePath& extensions_install_dir,
      bool autoupdate_enabled);

  void InitializeInstalledExtensionService(
      const FilePath& prefs_file, const FilePath& source_install_dir);

  void InitializeEmptyExtensionService();

  void InitializeExtensionProcessManager();

  void InitializeExtensionServiceWithUpdater();

  void InitializeRequestContext();

  static void SetUpTestCase();

  virtual void SetUp() OVERRIDE;

  void set_extensions_enabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

 protected:
  void InitializeExtensionServiceHelper(bool autoupdate_enabled);

  MessageLoop loop_;
  base::ShadowingAtExitManager at_exit_manager_;
  ScopedTempDir temp_dir_;
  scoped_ptr<TestingProfile> profile_;
  FilePath extensions_install_dir_;
  FilePath data_dir_;
  // Owned by |profile_|.
  ExtensionService* service_;
  size_t expected_extensions_count_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  content::TestBrowserThread webkit_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
