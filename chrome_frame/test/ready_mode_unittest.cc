// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atlwin.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/win/registry.h"
#include "chrome_frame/infobars/infobar_content.h"
#include "chrome_frame/ready_mode/internal/ready_mode_state.h"
#include "chrome_frame/ready_mode/internal/ready_prompt_content.h"
#include "chrome_frame/ready_mode/internal/ready_prompt_window.h"
#include "chrome_frame/ready_mode/internal/registry_ready_mode_state.h"
#include "chrome_frame/ready_mode/internal/url_launcher.h"
#include "chrome_frame/simple_resource_loader.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"

namespace {

class SetResourceInstance {
 public:
  SetResourceInstance() : res_dll_(NULL), old_res_dll_(NULL) {
    SimpleResourceLoader* loader_instance = SimpleResourceLoader::GetInstance();
    EXPECT_TRUE(loader_instance != NULL);
    if (loader_instance != NULL) {
      res_dll_ = loader_instance->GetResourceModuleHandle();
      EXPECT_TRUE(res_dll_ != NULL);
      if (res_dll_ != NULL) {
        old_res_dll_ = ATL::_AtlBaseModule.SetResourceInstance(res_dll_);
      }
    }
  }

  ~SetResourceInstance() {
    if (old_res_dll_ != NULL) {
      CHECK_EQ(res_dll_, ATL::_AtlBaseModule.SetResourceInstance(old_res_dll_));
    }
  }

 private:
  HMODULE res_dll_;
  HMODULE old_res_dll_;
};  // class SetResourceInstance

class SimpleWindow : public CWindowImpl<SimpleWindow,
                                        CWindow,
                                        CFrameWinTraits> {
 public:
  virtual ~SimpleWindow() {
    if (IsWindow())
      DestroyWindow();
  }

  static BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM l_param) {
    HWND* out = reinterpret_cast<HWND*>(l_param);
    EXPECT_TRUE(out != NULL);

    if (out == NULL)
      return FALSE;

    EXPECT_TRUE(*out == NULL || ::IsChild(*out, hwnd));

    if (*out == NULL)
      *out = hwnd;

    return TRUE;
  }

  HWND GetZeroOrOneChildWindows() {
    HWND child = NULL;
    EnumChildWindows(m_hWnd, EnumChildProc, reinterpret_cast<LPARAM>(&child));
    return child;
  }

  BEGIN_MSG_MAP(SimpleWindow)
  END_MSG_MAP()
};  // class SimpleWindow

class MockInfobarContentFrame : public InfobarContent::Frame {
 public:
  // InfobarContent::Frame implementation
  MOCK_METHOD0(GetFrameWindow, HWND(void));
  MOCK_METHOD0(CloseInfobar, void(void));
};  // class Frame

class MockReadyModeState : public ReadyModeState {
 public:
  // ReadyModeState implementation
  MOCK_METHOD0(TemporarilyDeclineChromeFrame, void(void));
  MOCK_METHOD0(PermanentlyDeclineChromeFrame, void(void));
  MOCK_METHOD0(AcceptChromeFrame, void(void));
};  // class MockReadyModeState

class MockUrlLauncher : public UrlLauncher {
 public:
  // UrlLauncher implementation
  MOCK_METHOD1(LaunchUrl, void(const std::wstring& url));
};  // class MockUrlLauncher

}  // namespace

class ReadyPromptTest : public testing::Test {
 public:
  ReadyPromptTest() : hwnd_(NULL) {}

  void SetUp() {
    hwnd_ = window_.Create(NULL);
    EXPECT_TRUE(hwnd_ != NULL);
    window_.ShowWindow(SW_SHOW);
    EXPECT_TRUE(window_.IsWindowVisible());
    EXPECT_CALL(frame_, GetFrameWindow()).Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(hwnd_));
  }

 protected:
  SimpleWindow window_;
  HWND hwnd_;
  MockInfobarContentFrame frame_;
  SetResourceInstance set_resource_instance_;
};  // class ReadyPromptTest

class ReadyPromptWindowTest : public ReadyPromptTest {
 public:
  void SetUp() {
    ReadyPromptTest::SetUp();

    // owned by ReadyPromptWindow
    state_ = new MockReadyModeState();
    url_launcher_ = new MockUrlLauncher();

    ready_prompt_window_ = ReadyPromptWindow::CreateInstance(
        &frame_, state_, url_launcher_);

    ASSERT_TRUE(ready_prompt_window_ != NULL);
    RECT position = {0, 0, 800, 39};
    ASSERT_TRUE(ready_prompt_window_->SetWindowPos(HWND_TOP, &position,
                                                   SWP_SHOWWINDOW));
  }

 protected:
  MockReadyModeState* state_;
  MockUrlLauncher* url_launcher_;
  base::WeakPtr<ReadyPromptWindow> ready_prompt_window_;
};  // class ReadyPromptWindowTest

class ReadyPromptWindowButtonTest : public ReadyPromptWindowTest {
 public:
  void TearDown() {
    ASSERT_TRUE(ready_prompt_window_ != NULL);
    ASSERT_TRUE(ready_prompt_window_->DestroyWindow());
    ASSERT_TRUE(ready_prompt_window_ == NULL);
    ASSERT_FALSE(message_loop_.WasTimedOut());

    ReadyPromptWindowTest::TearDown();
  }

 protected:
  struct ClickOnCaptionData {
    const wchar_t* target_caption;
    bool found;
  };  // struct ClickOnCaptionData

  static BOOL CALLBACK ClickOnCaptionProc(HWND hwnd, LPARAM l_param) {
    wchar_t window_caption[256] = {0};
    size_t buffer_length = arraysize(window_caption);

    ClickOnCaptionData* data = reinterpret_cast<ClickOnCaptionData*>(l_param);
    EXPECT_TRUE(data->target_caption != NULL);

    if (data->target_caption == NULL)
      return FALSE;

    if (wcsnlen(data->target_caption, buffer_length + 1) == buffer_length + 1)
      return FALSE;

    if (::GetWindowText(hwnd, window_caption, buffer_length) ==
        static_cast<int>(buffer_length)) {
      return TRUE;
    }

    if (wcscmp(data->target_caption, window_caption) == 0) {
      EXPECT_FALSE(data->found);

      CRect client_rect;
      EXPECT_TRUE(::GetClientRect(hwnd, client_rect));

      CPoint center_point(client_rect.CenterPoint());
      LPARAM coordinates = (center_point.y << 16) | center_point.x;

      ::PostMessage(hwnd, WM_LBUTTONDOWN, 0, coordinates);
      ::PostMessage(hwnd, WM_LBUTTONUP, 0, coordinates);

      data->found = true;
    }

    return TRUE;
  }

  bool ClickOnCaption(const std::wstring& caption) {
    ClickOnCaptionData data = {caption.c_str(), false};

    ::EnumChildWindows(hwnd_, ClickOnCaptionProc,
                       reinterpret_cast<LPARAM>(&data));
    return data.found;
  }

  void RunUntilCloseInfobar() {
    EXPECT_CALL(frame_, CloseInfobar()).WillOnce(QUIT_LOOP(message_loop_));
    ASSERT_NO_FATAL_FAILURE(message_loop_.RunFor(5));  // seconds
  }

  chrome_frame_test::TimedMsgLoop message_loop_;
};  // class ReadyPromptWindowButtonTest

TEST_F(ReadyPromptTest, ReadyPromptContentTest) {
  // owned by ReadyPromptContent
  MockReadyModeState* state = new MockReadyModeState();
  MockUrlLauncher* url_launcher = new MockUrlLauncher();

  scoped_ptr<ReadyPromptContent> content_(new ReadyPromptContent(state,
                                                                 url_launcher));

  content_->InstallInFrame(&frame_);

  // Ensure that, if a child is created, it is not visible yet.
  HWND child_hwnd = window_.GetZeroOrOneChildWindows();
  if (child_hwnd != NULL) {
    CWindow child(child_hwnd);
    RECT child_dimensions;
    EXPECT_TRUE(child.GetClientRect(&child_dimensions));
    EXPECT_FALSE(child.IsWindowVisible() && !::IsRectEmpty(&child_dimensions));
  }

  int desired_height = content_->GetDesiredSize(400, 0);
  EXPECT_GT(desired_height, 0);
  RECT dimensions = {10, 15, 410, 20};
  content_->SetDimensions(dimensions);

  child_hwnd = window_.GetZeroOrOneChildWindows();
  EXPECT_TRUE(child_hwnd != NULL);

  if (child_hwnd != NULL) {
    CWindow child(child_hwnd);
    EXPECT_TRUE(child.IsWindowVisible());
    RECT child_dimensions;
    EXPECT_TRUE(child.GetWindowRect(&child_dimensions));
    EXPECT_TRUE(window_.ScreenToClient(&child_dimensions));
    EXPECT_TRUE(::EqualRect(&child_dimensions, &dimensions));
  }

  // Being visible doesn't change the desired height
  EXPECT_EQ(desired_height, content_->GetDesiredSize(400, 0));

  content_.reset();

  EXPECT_TRUE(window_.GetZeroOrOneChildWindows() == NULL);
}

TEST_F(ReadyPromptWindowTest, Destroy) {
  // Should delete associated mocks, not invoke on ReadyModeState
  ready_prompt_window_->DestroyWindow();
}

TEST_F(ReadyPromptWindowButtonTest, ClickEnable) {
  EXPECT_CALL(*state_, AcceptChromeFrame());
  ASSERT_TRUE(ClickOnCaption(L"Enable"));
  RunUntilCloseInfobar();
}

TEST_F(ReadyPromptWindowButtonTest, ClickIgnore) {
  EXPECT_CALL(*state_, PermanentlyDeclineChromeFrame());
  ASSERT_TRUE(ClickOnCaption(L"Ignore"));
  RunUntilCloseInfobar();
}

// TODO(erikwright): test WebBrowserAdapter
// TODO(erikwright): an integration test of ReadyMode::Configure with a mock
//                   IWebBrowser2?
