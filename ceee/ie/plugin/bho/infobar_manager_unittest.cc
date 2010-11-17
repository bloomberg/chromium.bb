// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Infobar manager implementation unit tests.

// MockWin32 must not be included after atlwin, which is included by some
// headers in here, so we need to put it at the top:
#include "ceee/testing/utils/mock_win32.h"  // NOLINT

#include "ceee/ie/plugin/bho/infobar_manager.h"
#include "ceee/ie/plugin/bho/infobar_window.h"
#include "ceee/testing/utils/instance_count_mixin.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "broker_lib.h"  // NOLINT

namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

const HWND kGoodWindow = reinterpret_cast<HWND>(42);
const HWND kParentWindow = reinterpret_cast<HWND>(77);
const HWND kInfobarWindow = reinterpret_cast<HWND>(91);
const wchar_t* kUrl1 = L"/infobar/test.html";
const int kMaxHeight = 55;

class MockInfobarDelegate : public infobar_api::InfobarWindow::Delegate {
 public:
  MOCK_METHOD0(GetContainerWindow, HWND());
  MOCK_METHOD1(OnWindowClose, void(infobar_api::InfobarType));
};

class MockInfobarWindow : public infobar_api::InfobarWindow {
 public:
  MockInfobarWindow(infobar_api::InfobarType type, Delegate* delegate)
    : infobar_api::InfobarWindow(type, delegate) {}
  MOCK_METHOD2(InternalCreate, HWND(HWND, DWORD));
  MOCK_METHOD2(Show, HRESULT(int, bool));
  MOCK_METHOD0(Hide, HRESULT());
  MOCK_METHOD1(Navigate, HRESULT(const std::wstring& url));
  MOCK_METHOD0(Reset, void());

  HWND SetInfobarHwnd() {
    m_hWnd = kInfobarWindow;
    return m_hWnd;
  }
};

class TestingInfobarManager : public infobar_api::InfobarManager {
 public:
  class MockContainerWindow : public ContainerWindowInterface {
   public:
    // TODO(vadimb@google.com): Mock these two methods and test different
    // behaviors when they return different values.
    virtual bool IsDestroyed() const { return false; }
    virtual HWND GetWindowHandle() const { return kParentWindow; }
    MOCK_METHOD3(PostWindowsMessage, bool(UINT, WPARAM, LPARAM));
  };

  explicit TestingInfobarManager(HWND tab_window)
      : infobar_api::InfobarManager(tab_window) {
  }

  MockInfobarWindow* GetInfobarWindow(infobar_api::InfobarType type) {
    if (type < infobar_api::FIRST_INFOBAR_TYPE ||
        type >= infobar_api::END_OF_INFOBAR_TYPE) {
      return NULL;
    }
    return reinterpret_cast<MockInfobarWindow*>(infobars_[type].get());
  }

  void InitInfobarWindow(infobar_api::InfobarType type, Delegate* delegate) {
    ASSERT_TRUE(type >= infobar_api::FIRST_INFOBAR_TYPE &&
                type < infobar_api::END_OF_INFOBAR_TYPE);
    ASSERT_TRUE(infobars_[type] == NULL);
    infobars_[type].reset(new MockInfobarWindow(type, delegate));
  }

  virtual ContainerWindowInterface* CreateContainerWindow(
      HWND container, InfobarManager* manager) {
    return new MockContainerWindow;
  }

  void SetContainerWindow(ContainerWindowInterface* window) {
    container_window_.reset(window);
  }

  void EmulateOnClose(infobar_api::InfobarType type) {
    OnWindowClose(type);
    OnContainerWindowDelayedCloseInfobar(type);
  }
};

class InfobarManagerTests : public testing::Test {
 public:
  virtual void SetUp() {
    infobar_delegate_.reset(new MockInfobarDelegate);
    infobar_manager_.reset(new TestingInfobarManager(kGoodWindow));

    EXPECT_CALL(user32_, IsWindow(kParentWindow)).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(user32_, IsWindow(kGoodWindow)).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(user32_, IsWindow(kInfobarWindow)).WillRepeatedly(Return(TRUE));
    EXPECT_CALL(user32_, IsWindow(NULL)).WillRepeatedly(Return(FALSE));
    EXPECT_CALL(user32_, GetParent(_)).WillRepeatedly(Return(kGoodWindow));

    // Arbitrary number to return from GetWindowRect. Unique to make it easier
    // to trace in the debugger or to add in future tests dependent on return
    // values.
    RECT window_rect = {131, 213, 831, 1013};
    EXPECT_CALL(user32_, GetWindowRect(_, NotNull())).
        WillRepeatedly(DoAll(SetArgumentPointee<1>(window_rect), Return(TRUE)));
  }
  virtual void TearDown() {
    testing::LogDisabler no_dchecks;
    infobar_manager_.reset(NULL);
    infobar_delegate_.reset(NULL);

    // Everything should have been relinquished.
    ASSERT_EQ(0, testing::InstanceCountMixinBase::all_instance_count());
}

 protected:
  static BOOL MockEnumChildWindows(HWND, WNDENUMPROC, LPARAM lparam) {
    HWND* window_handle = reinterpret_cast<HWND*>(lparam);
    if (window_handle)
      *window_handle = kParentWindow;
    return TRUE;
  }

  StrictMock<testing::MockUser32> user32_;
  scoped_ptr<MockInfobarDelegate> infobar_delegate_;
  scoped_ptr<TestingInfobarManager> infobar_manager_;
};

TEST_F(InfobarManagerTests, GetContainerWindow) {
  EXPECT_CALL(user32_, EnumChildWindows(kGoodWindow, _, _)).
      WillOnce(Invoke(MockEnumChildWindows));

  ASSERT_EQ(kParentWindow, infobar_manager_->GetContainerWindow());
  // Next time it should not call EnumChildWindows.
  ASSERT_EQ(kParentWindow, infobar_manager_->GetContainerWindow());
}

TEST_F(InfobarManagerTests, ShowHide) {
  infobar_manager_->InitInfobarWindow(infobar_api::TOP_INFOBAR,
                                      infobar_delegate_.get());

  // Test Show first.
  MockInfobarWindow* infobar_window =
      infobar_manager_->GetInfobarWindow(infobar_api::TOP_INFOBAR);
  const std::wstring url(kUrl1);
  EXPECT_CALL(*infobar_window, Navigate(url)).WillOnce(Return(S_OK));
  EXPECT_CALL(*infobar_window, InternalCreate(kGoodWindow, _)).
      WillOnce(WithoutArgs(Invoke(infobar_window,
                                  &MockInfobarWindow::SetInfobarHwnd)));
  EXPECT_CALL(*infobar_window, Show(kMaxHeight, false));
  EXPECT_CALL(*infobar_delegate_, GetContainerWindow()).
      WillRepeatedly(Return(kParentWindow));
  EXPECT_CALL(user32_, SetWindowPos(_, _, _, _, _, _, _)).
      WillRepeatedly(Return(TRUE));

  EXPECT_HRESULT_SUCCEEDED(infobar_manager_->Show(infobar_api::TOP_INFOBAR,
      kMaxHeight, kUrl1, false));

  // Test Hide for this infobar two times. The second Hide should still be OK
  // even though the infobar is already hidden.
  EXPECT_CALL(*infobar_window, Reset()).Times(2);
  EXPECT_HRESULT_SUCCEEDED(infobar_manager_->Hide(infobar_api::TOP_INFOBAR));
  EXPECT_HRESULT_SUCCEEDED(infobar_manager_->Hide(infobar_api::TOP_INFOBAR));

  // However Hide to non-existent infobar should result in an error.
  EXPECT_HRESULT_FAILED(infobar_manager_->Hide(infobar_api::BOTTOM_INFOBAR));
  EXPECT_HRESULT_FAILED(infobar_manager_->Hide(
      static_cast<infobar_api::InfobarType>(55)));

  // HideAll hides only the existing infobar.
  EXPECT_CALL(*infobar_window, Reset());
  infobar_manager_->HideAll();
}

TEST_F(InfobarManagerTests, OnClose) {
  infobar_manager_->InitInfobarWindow(infobar_api::TOP_INFOBAR,
                                      infobar_delegate_.get());
  TestingInfobarManager::MockContainerWindow* container_window =
      new TestingInfobarManager::MockContainerWindow;
  infobar_manager_->SetContainerWindow(container_window);
  EXPECT_CALL(*container_window, PostWindowsMessage(_, _, _));
  MockInfobarWindow* infobar_window =
      infobar_manager_->GetInfobarWindow(infobar_api::TOP_INFOBAR);
  EXPECT_CALL(*infobar_window, Reset());
  infobar_manager_->EmulateOnClose(infobar_api::TOP_INFOBAR);
}

}  // namespace
