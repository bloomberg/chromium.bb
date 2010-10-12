// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_UNITTEST_H_
#pragma once

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionsServiceTestBase : public testing::Test {
 public:
  ExtensionsServiceTestBase();
  ~ExtensionsServiceTestBase();

  virtual void InitializeExtensionsService(
      const FilePath& pref_file, const FilePath& extensions_install_dir);

  virtual void InitializeInstalledExtensionsService(
      const FilePath& prefs_file, const FilePath& source_install_dir);

  virtual void InitializeEmptyExtensionsService();

  static void SetUpTestCase();

  virtual void SetUp();

  void set_extensions_enabled(bool enabled) {
    service_->set_extensions_enabled(enabled);
  }

 protected:
  ScopedTempDir temp_dir_;
  scoped_ptr<Profile> profile_;
  FilePath extensions_install_dir_;
  scoped_refptr<ExtensionsService> service_;
  size_t total_successes_;
  MessageLoop loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  BrowserThread webkit_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_UNITTEST_H_
