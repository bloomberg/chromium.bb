// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
// regular NPAPI plugins and pepper NPAPI plugins.
class NPAPITesterBase : public UITest {
 protected:
  explicit NPAPITesterBase();
  virtual void SetUp() OVERRIDE;

  FilePath GetPluginsDirectory();
};

// Helper class for NPAPI plugin UI tests, which need the browser window
// to be visible.
class NPAPIVisiblePluginTester : public NPAPITesterBase {
 protected:
  virtual void SetUp() OVERRIDE;
};

// Helper class for NPAPI plugin UI tests which use incognito mode.
class NPAPIIncognitoTester : public NPAPITesterBase {
 protected:
  virtual void SetUp() OVERRIDE;
};

#endif  // CHROME_TEST_UI_NPAPI_TEST_HELPER_H_
