// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_UNITTEST_H_

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chrome_thread.h"
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
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<Profile> profile_;
  FilePath extensions_install_dir_;
  scoped_refptr<ExtensionsService> service_;
  size_t total_successes_;
  MessageLoop loop_;
  ChromeThread ui_thread_;
  ChromeThread webkit_thread_;
  ChromeThread file_thread_;
  ChromeThread io_thread_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_SERVICE_UNITTEST_H_
