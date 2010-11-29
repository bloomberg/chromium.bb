// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Infobar window implementation unit tests.

// MockWin32 must not be included after atlwin, which is included by some
// headers in here, so we need to put it at the top:
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include "ceee/ie/plugin/bho/infobar_window.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "broker_lib.h"  // NOLINT

namespace {

using testing::_;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrEq;
using testing::StrictMock;

const HWND kGoodWindow = reinterpret_cast<HWND>(42);
const HWND kParentWindow = reinterpret_cast<HWND>(74);
const wchar_t* kUrl1 = L"/infobar/test.html";
const int kMaxHeight = 25;
const UINT_PTR timer_id = 19;

class MockInfobarBrowserWindow : public infobar_api::IInfobarBrowserWindow {
 public:
  MockInfobarBrowserWindow() : ref_count(0) {}
  ~MockInfobarBrowserWindow() { EXPECT_EQ(0, ref_count); }
  STDMETHOD_(ULONG, AddRef)() { return ++ref_count; }
  STDMETHOD_(ULONG, Release)() { return --ref_count; }
  STDMETHOD(QueryInterface)(REFIID, LPVOID*) { return S_OK; }

  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, CreateAndShowWindow, HRESULT(HWND));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, SetUrl, HRESULT(BSTR));
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, SetWindowSize, HRESULT(int, int));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Teardown, HRESULT());

  ULONG ref_count;
};

class MockInfobarWindowDelegate : public infobar_api::InfobarWindow::Delegate {
 public:
  MOCK_METHOD0(GetContainerWindow, HWND());
  MOCK_METHOD1(OnWindowClose, void(infobar_api::InfobarType));
};

class TestingInfobarWindow : public infobar_api::InfobarWindow {
 public:
  // Until we have a real use of BrokerRpc, we just pass a NULL pointer to
  // InfobarWindow.
  TestingInfobarWindow(infobar_api::InfobarType type,
                       InfobarWindow::Delegate* delegate,
                       MockInfobarBrowserWindow* browser_window)
      : infobar_api::InfobarWindow(type, delegate, NULL) {
    chrome_frame_host_ = browser_window;
  }
};

class InfobarWindowTests : public testing::Test {
 public:
  virtual void SetUp() {
    infobar_window_delegate_.reset(new StrictMock<MockInfobarWindowDelegate>);
    browser_window_.reset(new StrictMock<MockInfobarBrowserWindow>);
    infobar_window_.reset(new StrictMock<TestingInfobarWindow>(
        infobar_api::TOP_INFOBAR, infobar_window_delegate_.get(),
        browser_window_.get()));
    infobar_window_->m_hWnd = kGoodWindow;

    EXPECT_CALL(*browser_window_, Teardown()).WillOnce(Return(S_OK));
    EXPECT_CALL(*infobar_window_delegate_, GetContainerWindow()).
        WillRepeatedly(Return(kParentWindow));

    EXPECT_CALL(user32_, IsWindow(kGoodWindow)).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(user32_, IsWindow(kParentWindow)).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(user32_, IsWindow(NULL)).WillRepeatedly(Return(FALSE));
    EXPECT_CALL(user32_, GetParent(_)).WillRepeatedly(Return(kParentWindow));

    // Arbitrary number to return from GetWindowRect. Unique to make it easier
    // to trace in the debugger or to add in future tests dependent on return
    // values.
    RECT window_rect = {131, 213, 831, 1013};
    EXPECT_CALL(user32_, GetWindowRect(kParentWindow, NotNull())).
        WillRepeatedly(DoAll(SetArgumentPointee<1>(window_rect), Return(TRUE)));
    EXPECT_CALL(user32_, SetWindowPos(_, _, _, _, _, _, _)).
        WillRepeatedly(Return(TRUE));
  }
  virtual void TearDown() {
    testing::LogDisabler no_dchecks;
    // Infobar window must be deleted before delegate as the destructor will
    // call the delegate member functions.
    infobar_window_.reset(NULL);
    infobar_window_delegate_.reset(NULL);

    // Everything should have been relinquished.
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
}

 protected:
  StrictMock<testing::MockUser32> user32_;
  scoped_ptr<StrictMock<MockInfobarWindowDelegate>> infobar_window_delegate_;
  scoped_ptr<StrictMock<TestingInfobarWindow>> infobar_window_;
  scoped_ptr<StrictMock<MockInfobarBrowserWindow>> browser_window_;
};

TEST_F(InfobarWindowTests, ShowHide) {
  // Show without previous Navigate should fail.
  EXPECT_HRESULT_FAILED(infobar_window_->Show(kMaxHeight, false));

  // Navigate, then Show.
  EXPECT_CALL(*browser_window_, SetUrl(StrEq(kUrl1))).Times(1);
  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Navigate(std::wstring(kUrl1)));
  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Show(kMaxHeight, false));
  EXPECT_EQ(kMaxHeight, infobar_window_->target_height_);
  EXPECT_EQ(kMaxHeight, infobar_window_->current_height_);
  EXPECT_TRUE(infobar_window_->show_);

  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Hide());
  EXPECT_FALSE(infobar_window_->show_);
}

TEST_F(InfobarWindowTests, SlidingShow) {
  EXPECT_CALL(*browser_window_, SetUrl(StrEq(kUrl1))).Times(1);
  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Navigate(std::wstring(kUrl1)));
  EXPECT_CALL(user32_, SetTimer(kGoodWindow, _, _, _)).
      WillRepeatedly(Return(timer_id));
  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Show(kMaxHeight, true));
  EXPECT_EQ(kMaxHeight, infobar_window_->target_height_);
  // With sliding enabled, the hight of the infobar should not be the target
  // height immediately - it increases on each timer tick until reaches target.
  EXPECT_LE(infobar_window_->current_height_, kMaxHeight);
  EXPECT_TRUE(infobar_window_->show_);
  EXPECT_TRUE(infobar_window_->sliding_infobar_);

  // Call timer callback function until required.
  EXPECT_CALL(user32_, KillTimer(kGoodWindow, timer_id)).WillOnce(Return(TRUE));
  // Restrict the maximum number of times the loop can run so the test does not
  // hang if infobar_window_->sliding_infobar_ is not set. Normally it should
  // run just a few times (3 times if kMaxHeight is 25 and sliding step is 10).
  for (int i = 0; infobar_window_->sliding_infobar_ && i < 100; ++i) {
    infobar_window_->OnTimer(timer_id);
  }
  // Check that the loop is exited because of sliding_infobar_ condition and not
  // the counter saturation.
  EXPECT_FALSE(infobar_window_->sliding_infobar_);
  EXPECT_EQ(kMaxHeight, infobar_window_->current_height_);
  EXPECT_TRUE(infobar_window_->show_);
}

TEST_F(InfobarWindowTests, OnClose) {
  EXPECT_CALL(*infobar_window_delegate_,
              OnWindowClose(infobar_api::TOP_INFOBAR)).Times(1);

  infobar_window_->OnBrowserWindowClose();
}

TEST_F(InfobarWindowTests, ReserveSpace) {
  // Arbitrary number to return from GetWindowRect. Unique to make it easier
  // to trace in the debugger.
  CRect rect1(255, 311, 814, 1015);
  CRect rect0 = rect1;
  infobar_window_->ReserveSpace(&rect1);
  EXPECT_EQ(rect0, rect1);

  EXPECT_CALL(*browser_window_, SetUrl(StrEq(kUrl1))).Times(1);
  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Navigate(std::wstring(kUrl1)));
  EXPECT_HRESULT_SUCCEEDED(infobar_window_->Show(kMaxHeight, false));
  infobar_window_->ReserveSpace(&rect1);
  EXPECT_EQ(rect0.left, rect1.left);
  EXPECT_EQ(rect0.right, rect1.right);
  EXPECT_EQ(rect0.top + kMaxHeight, rect1.top);
  EXPECT_EQ(rect0.bottom, rect1.bottom);
}

}  // namespace
