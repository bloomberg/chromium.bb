// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_NPAPI_TEST_HELPER_H_
#define CHROME_TEST_UI_NPAPI_TEST_HELPER_H_
#pragma once

#include "chrome/test/ui/ui_test.h"

namespace npapi_test {
extern const char kTestCompleteCookie[];
extern const char kTestCompleteSuccess[];
}  // namespace npapi_test.

// Base class for NPAPI tests. It provides common functionality between
// regular NPAPI plugins and pepper NPAPI plugins. The base classes provide the
// name of the plugin they need test in the constructor. This base class will
// copy the plugin (assuming it has been built) to the plugins directory
// so it is loaded when chromium is launched.
class NPAPITesterBase : public UITest {
 protected:
  explicit NPAPITesterBase(const std::string& test_plugin_name);
  virtual void SetUp();
  virtual void TearDown();

  FilePath GetPluginsDirectory();

 private:
  std::string test_plugin_name_;
  FilePath test_plugin_path_;
};

// Helper class for NPAPI plugin UI tests.
class NPAPITester : public NPAPITesterBase {
 protected:
  NPAPITester();
  virtual void SetUp();
  virtual void TearDown();

 private:
#if defined(OS_MACOSX)
  FilePath layout_plugin_path_;
#endif  // OS_MACOSX
};

// Helper class for NPAPI plugin UI tests, which need the browser window
// to be visible.
class NPAPIVisiblePluginTester : public NPAPITester {
 protected:
  virtual void SetUp();
};

// Helper class for NPAPI plugin UI tests which use incognito mode.
class NPAPIIncognitoTester : public NPAPITester {
 protected:
  virtual void SetUp();
};

#endif  // CHROME_TEST_UI_NPAPI_TEST_HELPER_H_
