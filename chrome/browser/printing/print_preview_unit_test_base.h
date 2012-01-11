// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_UNIT_TEST_BASE_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_UNIT_TEST_BASE_H_
#pragma once

#include "chrome/test/base/browser_with_test_window_test.h"

class PrintPreviewUnitTestBase : public BrowserWithTestWindowTest {
 public:
  PrintPreviewUnitTestBase();
  virtual ~PrintPreviewUnitTestBase();

 protected:
  virtual void SetUp() OVERRIDE;
};

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_UNIT_TEST_BASE_H_
