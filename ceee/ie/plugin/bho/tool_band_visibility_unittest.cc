// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for ToolBandVisibility.

#include "ceee/common/initializing_coclass.h"
#include "ceee/ie/common/mock_ceee_module_util.h"
#include "ceee/ie/plugin/bho/tool_band_visibility.h"
#include "ceee/testing/utils/mock_com.h"
#include "gtest/gtest.h"

namespace {
using testing::_;
using testing::Return;
using testing::StrictMock;

class TestingToolBandVisibility : public ToolBandVisibility {
 public:
  TestingToolBandVisibility() {}
  virtual ~TestingToolBandVisibility() {}

  IWebBrowser2* GetWindowBrowser() const {
    return web_browser_;
  }

  using ToolBandVisibility::IsToolBandVisible;
  using ToolBandVisibility::ClearCachedVisibility;
  using ToolBandVisibility::CheckToolBandVisibility;
  using ToolBandVisibility::OnTimer;

  MOCK_METHOD0(CreateNotificationWindow, bool());
  MOCK_METHOD0(CloseNotificationWindow, void());
  MOCK_METHOD2(SetWindowTimer, void(UINT, UINT));
  MOCK_METHOD1(KillWindowTimer, void(UINT));
};

class MockBrowser
    : public testing::MockIWebBrowser2,
      public InitializingCoClass<MockBrowser> {
 public:
  HRESULT Initialize(MockBrowser** browser) {
    *browser = this;
    return S_OK;
  }
};

class ToolBandVisibilityTest : public testing::Test {
 public:
  virtual void TearDown() {
    TestingToolBandVisibility::ClearCachedVisibility(NULL);
  }

  void CreateMockBrowser(MockBrowser** browser, IWebBrowser2** browser_keeper) {
    ASSERT_TRUE(browser && browser_keeper);
    ASSERT_HRESULT_SUCCEEDED(
        MockBrowser::CreateInitialized(browser, browser_keeper));
  }

  void ExpectCheckToolBandVisibilitySucceeded(
      TestingToolBandVisibility* visibility) {
    EXPECT_CALL(ceee_module_utils_, GetOptionToolbandIsHidden())
        .WillOnce(Return(false));
    EXPECT_CALL(*visibility, CreateNotificationWindow())
        .WillOnce(Return(true));
    EXPECT_CALL(*visibility, SetWindowTimer(1, 2000)).Times(1);
  }

  StrictMock<testing::MockCeeeModuleUtils> ceee_module_utils_;
  TestingToolBandVisibility visibility;
};

TEST_F(ToolBandVisibilityTest, ReportToolBandVisibleSucceeds) {
  MockBrowser* browser1;
  MockBrowser* browser2;
  MockBrowser* browser3;
  CComPtr<IWebBrowser2> browser1_keeper, browser2_keeper, browser3_keeper;
  CreateMockBrowser(&browser1, &browser1_keeper);
  CreateMockBrowser(&browser2, &browser2_keeper);
  CreateMockBrowser(&browser3, &browser3_keeper);

  ASSERT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser1_keeper));
  ASSERT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser2_keeper));
  ASSERT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser3_keeper));

  TestingToolBandVisibility::ReportToolBandVisible(browser2_keeper);
  EXPECT_TRUE(TestingToolBandVisibility::IsToolBandVisible(browser2_keeper));
  ASSERT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser1_keeper));
  ASSERT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser3_keeper));

  TestingToolBandVisibility::ReportToolBandVisible(browser3_keeper);
  TestingToolBandVisibility::ClearCachedVisibility(browser2_keeper);

  EXPECT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser1_keeper));
  EXPECT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser2_keeper));
  EXPECT_TRUE(TestingToolBandVisibility::IsToolBandVisible(browser3_keeper));

  // Clearing visibility for a browser that isn't visible is essentially a
  // no-op.
  TestingToolBandVisibility::ClearCachedVisibility(browser1_keeper);
  EXPECT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser1_keeper));
  ASSERT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser2_keeper));
  ASSERT_TRUE(TestingToolBandVisibility::IsToolBandVisible(browser3_keeper));
}

TEST_F(ToolBandVisibilityTest, CheckToolBandVisibilityHiddenToolband) {
  MockBrowser* browser;
  CComPtr<IWebBrowser2> browser_keeper;
  CreateMockBrowser(&browser, &browser_keeper);
  TestingToolBandVisibility visibility;

  EXPECT_CALL(ceee_module_utils_, GetOptionToolbandIsHidden())
      .WillOnce(Return(true));
  EXPECT_CALL(visibility, CreateNotificationWindow()).Times(0);
  visibility.CheckToolBandVisibility(browser_keeper);
  EXPECT_EQ(browser_keeper, visibility.GetWindowBrowser());
}

TEST_F(ToolBandVisibilityTest, CheckToolBandVisibilityCreateFailed) {
  MockBrowser* browser;
  CComPtr<IWebBrowser2> browser_keeper;
  CreateMockBrowser(&browser, &browser_keeper);
  TestingToolBandVisibility visibility;

  EXPECT_CALL(ceee_module_utils_, GetOptionToolbandIsHidden())
      .WillOnce(Return(false));
  EXPECT_CALL(visibility, CreateNotificationWindow())
      .WillOnce(Return(false));
  EXPECT_CALL(visibility, SetWindowTimer(_, _)).Times(0);
  visibility.CheckToolBandVisibility(browser_keeper);
}

TEST_F(ToolBandVisibilityTest, CheckToolBandVisibilitySucceeded) {
  MockBrowser* browser;
  CComPtr<IWebBrowser2> browser_keeper;
  CreateMockBrowser(&browser, &browser_keeper);
  TestingToolBandVisibility visibility;

  ExpectCheckToolBandVisibilitySucceeded(&visibility);
  visibility.CheckToolBandVisibility(browser_keeper);
}

TEST_F(ToolBandVisibilityTest, OnTimerVisibleToolBand) {
  MockBrowser* browser;
  CComPtr<IWebBrowser2> browser_keeper;
  CreateMockBrowser(&browser, &browser_keeper);
  TestingToolBandVisibility visibility;

  TestingToolBandVisibility::ReportToolBandVisible(browser_keeper);
  EXPECT_TRUE(TestingToolBandVisibility::IsToolBandVisible(browser_keeper));

  ExpectCheckToolBandVisibilitySucceeded(&visibility);
  visibility.CheckToolBandVisibility(browser_keeper);

  EXPECT_CALL(visibility, KillWindowTimer(1)).Times(1);
  EXPECT_CALL(visibility, CloseNotificationWindow()).Times(1);
  visibility.OnTimer(1);

  EXPECT_FALSE(TestingToolBandVisibility::IsToolBandVisible(browser_keeper));
}

TEST_F(ToolBandVisibilityTest, OnTimerInvisibleToolBand) {
  MockBrowser* browser;
  CComPtr<IWebBrowser2> browser_keeper;
  CreateMockBrowser(&browser, &browser_keeper);
  TestingToolBandVisibility visibility;

  ExpectCheckToolBandVisibilitySucceeded(&visibility);
  visibility.CheckToolBandVisibility(browser_keeper);

  EXPECT_CALL(visibility, KillWindowTimer(1)).Times(1);
  EXPECT_CALL(ceee_module_utils_, SetIgnoreShowDWChanges(true)).Times(1);
  EXPECT_CALL(*browser, ShowBrowserBar(_, _, _)).Times(3);
  EXPECT_CALL(ceee_module_utils_, SetIgnoreShowDWChanges(false)).Times(1);
  EXPECT_CALL(visibility, CloseNotificationWindow()).Times(1);
  visibility.OnTimer(1);
}

}  // namespace
