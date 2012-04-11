// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/mock_ie_event_sink_actions.h"
#include "chrome_frame/test/mock_ie_event_sink_test.h"

using testing::_;
using testing::StrEq;

namespace chrome_frame_test {

// Test fixture for compatibility/reliability tests.
class ChromeFrameSitesTest
    : public MockIEEventSinkTest,
      public testing::TestWithParam<std::wstring> {
 public:
  ChromeFrameSitesTest() {}

  virtual void SetUp() {
    // Permit navigation in both IE and CF.
    ie_mock_.ExpectAnyNavigations();
  }
};

INSTANTIATE_TEST_CASE_P(CF, ChromeFrameSitesTest,
                        testing::Values(L"http://www.meebo.com/",
                                        L"http://www.vimeo.com/",
                                        L"http://wordpress.com/",
                                        L"https://github.com/"));

// Test for navigating to a site that has a CF metatag.
TEST_P(ChromeFrameSitesTest, LoadSite) {
  // Print name of site for debugging purposes.
  std::wcout << L"Navigating to site: " << GetParam() << std::endl;

  // Verify navigation to the url passed in as parameter.
  EXPECT_CALL(ie_mock_, OnLoad(IN_CF, StrEq(GetParam())))
      .WillOnce(testing::DoAll(
          VerifyAddressBarUrl(&ie_mock_),
          CloseBrowserMock(&ie_mock_)));

  LaunchIENavigateAndLoop(GetParam(), kChromeFrameLongNavigationTimeout * 2);
}

}  // namespace chrome_frame_test
