// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/external_tab.h"

// #include "base/synchronization/waitable_event.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/threading/thread.h"
#include "chrome/common/automation_messages.h"
#include "chrome_frame/navigation_constraints.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

using testing::StrictMock;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::CreateFunctor;
using testing::Return;
using testing::DoAll;
using testing::Field;
using chrome_frame_test::TimedMsgLoop;

struct MockUIDelegate : public UIDelegate {
  MOCK_METHOD2(OnNavigationStateChanged, void(int flags,
      const NavigationInfo& nav_info));
  MOCK_METHOD1(OnUpdateTargetUrl, void(const std::wstring& new_target_url));
  MOCK_METHOD1(OnLoad, void(const GURL& url));
  MOCK_METHOD1(OnMoveWindow, void(const gfx::Rect& pos));
  MOCK_METHOD3(OnMessageFromChromeFrame, void(const std::string& message,
      const std::string& origin, const std::string& target));
  MOCK_METHOD3(OnHandleContextMenu, void(const ContextMenuModel& menu_model,
      int align_flags, const MiniContextMenuParams& params));
  MOCK_METHOD1(OnHandleAccelerator, void(const MSG& accel_message));
  MOCK_METHOD1(OnTabbedOut, void(bool reverse));
  MOCK_METHOD1(OnGoToHistoryOffset, void(int offset));
  MOCK_METHOD3(OnOpenURL, void(const GURL& url_to_open, const GURL& referrer,
      int open_disposition));
};

struct MockProxy : public ChromeProxy {
  MOCK_METHOD1(RemoveBrowsingData, void(int remove_mask));
  MOCK_METHOD1(SetProxyConfig, void(const std::string& json_encoded_settings));

  MOCK_METHOD2(CreateTab, void(ChromeProxyDelegate* delegate,
      const ExternalTabSettings& settings));
  MOCK_METHOD3(ConnectTab, void(ChromeProxyDelegate* delegate, HWND hwnd,
      uint64 cookie));
  MOCK_METHOD1(BlockTab, void(uint64 cookie));

  MOCK_METHOD4(Tab_PostMessage, void(int tab, const std::string& message,
      const std::string& origin, const std::string& target));
  MOCK_METHOD1(Tab_Reload, void(int tab));
  MOCK_METHOD1(Tab_Stop, void(int tab));
  MOCK_METHOD1(Tab_SaveAs, void(int tab));
  MOCK_METHOD1(Tab_Print, void(int tab));
  MOCK_METHOD1(Tab_Cut, void(int tab));
  MOCK_METHOD1(Tab_Copy, void(int tab));
  MOCK_METHOD1(Tab_Paste, void(int tab));
  MOCK_METHOD1(Tab_SelectAll, void(int tab));
  MOCK_METHOD5(Tab_Find, void(int tab, const string16& search_string,
      FindInPageDirection forward, FindInPageCase match_case, bool find_next));
  MOCK_METHOD2(Tab_MenuCommand, void(int tab, int selected_command));

  MOCK_METHOD2(Tab_Zoom, void(int tab, content::PageZoom zoom_level));
  MOCK_METHOD2(Tab_FontSize, void(int tab, AutomationPageFontSize font_size));
  MOCK_METHOD3(Tab_SetInitialFocus, void(int tab, bool reverse,
      bool restore_focus_to_view));
  MOCK_METHOD1(Tab_SetParentWindow, void(int tab));
  MOCK_METHOD1(Tab_Resize, void(int tab));
  MOCK_METHOD2(Tab_ProcessAccelerator, void(int tab, const MSG& msg));

  // Misc.
  MOCK_METHOD1(Tab_OnHostMoved, void(int tab));
  MOCK_METHOD1(Tab_RunUnloadHandlers, void(int tab));
  MOCK_METHOD3(Tab_Navigate, void(int tab, const GURL& url,
      const GURL& referrer));
  MOCK_METHOD2(Tab_OverrideEncoding, void(int tab, const char* encoding));

  MOCK_METHOD1(Init, void(const ProxyParams& params));
  MOCK_METHOD1(AddDelegate, int(ChromeProxyDelegate* delegate));
  MOCK_METHOD1(RemoveDelegate, int(ChromeProxyDelegate* delegate));
};

struct MockFactory : public ChromeProxyFactory {
  MOCK_METHOD0(CreateProxy, ChromeProxy*());
};


// This class just calls methods of ChromeProxyDelegate but in background thread
struct AsyncEventCreator {
 public:
  explicit AsyncEventCreator(ChromeProxyDelegate* delegate)
      : delegate_(delegate), ipc_thread_("ipc") {
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    ipc_thread_.StartWithOptions(options);
    ipc_loop_ = ipc_thread_.message_loop();
  }

  void Fire_Connected(ChromeProxy* proxy, base::TimeDelta delay) {
    ipc_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromeProxyDelegate::Connected, base::Unretained(delegate_),
                   proxy),
        delay.InMilliseconds());
  }

  void Fire_PeerLost(ChromeProxy* proxy,
      ChromeProxyDelegate::DisconnectReason reason, base::TimeDelta delay) {
    ipc_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromeProxyDelegate::PeerLost, base::Unretained(delegate_),
                   proxy, reason),
        delay.InMilliseconds());
  }

  void Fire_Disconnected(base::TimeDelta delay) {
    ipc_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromeProxyDelegate::Disconnected,
                   base::Unretained(delegate_)),
        delay.InMilliseconds());
  }

  void Fire_CompletedCreateTab(bool success, HWND chrome_wnd, HWND tab_window,
                               int tab_handle, int session_id,
                               base::TimeDelta delay) {
    ipc_loop_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ChromeProxyDelegate::Completed_CreateTab,
                   base::Unretained(delegate_), success, chrome_wnd, tab_window,
                   tab_handle, session_id),
        delay.InMilliseconds());
  }

 private:
  ChromeProxyDelegate* delegate_;
  base::Thread ipc_thread_;
  MessageLoop* ipc_loop_;
};

// We may want the same test with 'real' proxy and mock 'proxy traits'.
TEST(ExternalTabProxy, CancelledCreateTab) {
  MockUIDelegate ui_delegate;
  StrictMock<MockFactory> factory;
  scoped_ptr<ExternalTabProxy> tab(new ExternalTabProxy());
  AsyncEventCreator async_events(tab.get());
  StrictMock<MockProxy>* proxy = new StrictMock<MockProxy>();
  TimedMsgLoop loop;
  tab->set_proxy_factory(&factory);

  EXPECT_CALL(factory, CreateProxy()).WillOnce(Return(proxy));
  EXPECT_CALL(*proxy, Init(_));
  EXPECT_CALL(*proxy, AddDelegate(tab.get())).WillOnce(DoAll(InvokeWithoutArgs(
      CreateFunctor(&async_events, &AsyncEventCreator::Fire_Connected,
      proxy, base::TimeDelta::FromMilliseconds(30))),
      Return(1)));

  EXPECT_CALL(*proxy, CreateTab(tab.get(), _))
      .WillOnce(QUIT_LOOP(loop));

  EXPECT_CALL(*proxy, RemoveDelegate(_)).WillOnce(DoAll(
      InvokeWithoutArgs(CreateFunctor(&async_events,
          &AsyncEventCreator::Fire_CompletedCreateTab, false, HWND(0), HWND(0),
          0, 0, base::TimeDelta::FromMilliseconds(0))),
      InvokeWithoutArgs(CreateFunctor(&async_events,
          &AsyncEventCreator::Fire_Disconnected,
          base::TimeDelta::FromMilliseconds(0))),
      Return(0)));

  CreateTabParams tab_params;
  tab_params.is_incognito = true;
  tab_params.is_widget_mode = false;
  tab_params.url = GURL("http://Xanadu.org");

  tab->CreateTab(tab_params, &ui_delegate);
  loop.RunFor(5);
  EXPECT_FALSE(loop.WasTimedOut());
  tab.reset();
}
