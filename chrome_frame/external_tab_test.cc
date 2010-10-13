// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/external_tab.h"
// #include "base/tracked.h"
// #include "base/task.h"
// #include "base/waitable_event.h"

#include "chrome/test/automation/automation_messages.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"



// DISABLE_RUNNABLE_METHOD_REFCOUNT(ExternalTabProxy);
// DISABLE_RUNNABLE_METHOD_REFCOUNT(UIDelegate);


struct MockUIDelegate : public UIDelegate {
  MOCK_METHOD2(OnNavigationStateChanged, void(int flags,
      const IPC::NavigationInfo& nav_info));
  MOCK_METHOD1(OnUpdateTargetUrl, void(const std::wstring& new_target_url));
  MOCK_METHOD3(OnExtensionInstalled, void(const FilePath& path, void* user_data,
      AutomationMsg_ExtensionResponseValues response));
  MOCK_METHOD1(OnLoad, void(const GURL& url));
  MOCK_METHOD3(OnMessageFromChromeFrame, void(const std::string& message,
      const std::string& origin, const std::string& target));
  MOCK_METHOD3(OnHandleContextMenu, void(HANDLE menu_handle, int align_flags,
      const IPC::ContextMenuParams& params));
  MOCK_METHOD1(OnHandleAccelerator, void(const MSG& accel_message));
  MOCK_METHOD1(OnTabbedOut, void(bool reverse));
  MOCK_METHOD1(OnGoToHistoryOffset, void(int offset));
  MOCK_METHOD3(OnOpenURL, void(const GURL& url_to_open, const GURL& referrer,
      int open_disposition));
};

struct MockProxy : public ChromeProxy {
  MOCK_METHOD1(RemoveBrowsingData, void(int remove_mask));
  MOCK_METHOD3(InstallExtension, void(ChromeProxyDelegate* delegate,
      const FilePath& crx_path, SyncMessageContext* ctx));
  MOCK_METHOD3(LoadExtension, void(ChromeProxyDelegate* delegate,
      const FilePath& path, SyncMessageContext* ctx));
  MOCK_METHOD2(GetEnabledExtensions, void(ChromeProxyDelegate* delegate,
      SyncMessageContext* ctx));
  MOCK_METHOD1(SetProxyConfig, void(const std::string& json_encoded_settings));

  MOCK_METHOD2(CreateTab, void(ChromeProxyDelegate* delegate,
      const IPC::ExternalTabSettings& settings));
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

  MOCK_METHOD2(Tab_Zoom, void(int tab, PageZoom::Function zoom_level));
  MOCK_METHOD2(Tab_FontSize, void(int tab, AutomationPageFontSize font_size));
  MOCK_METHOD3(Tab_SetInitialFocus, void(int tab, bool reverse,
      bool restore_focus_to_view));
  MOCK_METHOD1(Tab_SetParentWindow, void(int tab));
  MOCK_METHOD1(Tab_Resize, void(int tab));
  MOCK_METHOD2(Tab_ProcessAccelerator, void(int tab, const MSG& msg));

  // Misc.
  MOCK_METHOD1(Tab_OnHostMoved, void(int tab));
  MOCK_METHOD1(Tab_RunUnloadHandlers, void(int tab));
  MOCK_METHOD2(Tab_SetEnableExtensionAutomation, void(int tab,
      const std::vector<std::string>& functions_enabled));
  MOCK_METHOD3(Tab_Navigate, void(int tab, const GURL& url,
      const GURL& referrer));
  MOCK_METHOD2(Tab_OverrideEncoding, void(int tab, const char* encoding));

 private:
  MOCK_METHOD1(Init, void(const ProxyParams& params));
  MOCK_METHOD1(AddDelegate, int(ChromeProxyDelegate* delegate));
  MOCK_METHOD1(RemoveDelegate, int(ChromeProxyDelegate* delegate));
};

TEST(ExternalTabProxy, Simple1) {
  MockUIDelegate ui;
  MockProxy proxy;
  ExternalTabProxy tab;
}
