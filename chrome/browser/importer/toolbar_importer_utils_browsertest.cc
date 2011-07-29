// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/importer/toolbar_importer_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {

class ToolbarImporterUtilsTest : public InProcessBrowserTest {
 public:
  ToolbarImporterUtilsTest() : did_run_(false) {
  }

  void Callback(bool result) {
    DCHECK(!result);
    did_run_ = true;
    MessageLoop::current()->Quit();
  }

 protected:
  bool did_run_;
};

IN_PROC_BROWSER_TEST_F(ToolbarImporterUtilsTest, NoCrash) {
  // Regression test for http://crbug.com/89752
  toolbar_importer_utils::IsGoogleGAIACookieInstalled(
      base::Bind(&ToolbarImporterUtilsTest::Callback,
                 base::Unretained(this)),
      browser()->profile());
  if (!did_run_) {
    ui_test_utils::RunMessageLoop();
  }
  ASSERT_TRUE(did_run_);
}

}  // namespace
