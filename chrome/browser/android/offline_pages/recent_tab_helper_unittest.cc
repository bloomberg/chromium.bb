// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/recent_tab_helper.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class RecentTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  RecentTabHelperTest() {}
  ~RecentTabHelperTest() override {}

  void SetUp() override;

  RecentTabHelper* recent_tab_helper() const {
    return recent_tab_helper_;
  }

 private:
  RecentTabHelper* recent_tab_helper_;  // Owned by WebContents.

  DISALLOW_COPY_AND_ASSIGN(RecentTabHelperTest);
};

void RecentTabHelperTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  RecentTabHelper::CreateForWebContents(web_contents());
  recent_tab_helper_ =
      RecentTabHelper::FromWebContents(web_contents());
}

TEST_F(RecentTabHelperTest, Basic) {
  EXPECT_NE(nullptr, recent_tab_helper());
}

}  // namespace offline_pages
