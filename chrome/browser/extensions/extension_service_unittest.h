// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  void InitializeExtensionServiceWithUpdater();

  static void SetUpTestCase();

  virtual void SetUp();

  void set_extensions_enabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

 protected:
  void InitializeExtensionServiceHelper(bool autoupdate_enabled);

  ScopedTempDir temp_dir_;
  scoped_ptr<Profile> profile_;
  FilePath extensions_install_dir_;
  FilePath data_dir_;
  scoped_refptr<ExtensionService> service_;
  size_t total_successes_;
  MessageLoop loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  BrowserThread webkit_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SERVICE_UNITTEST_H_
