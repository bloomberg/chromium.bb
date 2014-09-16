// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_CHROME_MANIFEST_TEST_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_CHROME_MANIFEST_TEST_H_

#include "base/macros.h"
#include "extensions/common/manifest_test.h"

// Base class for unit tests that load manifest data from Chrome TEST_DATA_DIR.
// TODO(jamescook): Move this class and all subclasses into the extensions
// namespace.
class ChromeManifestTest : public extensions::ManifestTest {
 public:
  ChromeManifestTest();
  virtual ~ChromeManifestTest();

  // ManifestTest overrides:
  virtual base::FilePath GetTestDataDir() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeManifestTest);
};

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_TESTS_CHROME_MANIFEST_TEST_H_
