// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class StatusAreaViewTest : public InProcessBrowserTest {
 protected:
  StatusAreaViewTest() : InProcessBrowserTest() {}
  StatusAreaView* GetStatusAreaView() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<StatusAreaView*>(view->GetViewByID(VIEW_ID_STATUS_AREA));
  }
};

IN_PROC_BROWSER_TEST_F(StatusAreaViewTest, VisibleTest) {
  StatusAreaView* status = GetStatusAreaView();
  EXPECT_TRUE(status->IsVisibleInRootView());
  EXPECT_FALSE(status->size().IsEmpty());
}

}  // namespace chromeos
