// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CFPROXY_H_
#define CHROME_FRAME_CFPROXY_H_
#pragma once

#include <windows.h>

#include <map>  // for proxy factory
#include <vector>
#include <string>

#include "base/synchronization/lock.h"
#include "base/time.h"         // for base::TimeDelta
#include "base/file_path.h"
#include "chrome/common/automation_constants.h"
#include "content/public/common/page_zoom.h"
#include "ipc/ipc_channel.h"

class ChromeProxyDelegate;
class ChromeProxyFactory;
class GURL;
struct ExternalTabSettings;
struct ProxyParams;

// Some callers of synchronous messages wants a context to be passed back
// in order to identify the call they made. Presumably one can make
// multiple sync calls of same type (as async) and want to identify what
// is what.
struct SyncMessageContext {
  virtual ~SyncMessageContext() {}
};


/*
[activex]       UIDelegate (UI_THREAD)
[activedoc]  <---------------+
                             |
                             |            ChromeProxy (UI_THREAD)
                       +----------------+ -------------->   +-------+
 URL_FETCHER <---------|ExternalTabProxy|                   |CFProxy|
                       +----------------+                   +-------+
                                                               |
                                 ^                             |
                                 |                             |
                                 +-----------------------------+

                                  ChromeProxyDelegate (IPC_THREAD)

*/

// ChromeProxy is an abstract class. Is forwards the commands to an
// instance of the running Chromium browser.
// A pointer to ChromeProxy instance is obtained through a
// ChromeProxyFactory object.
class ChromeProxy {
 public:
  // General
  virtual void RemoveBrowsingData(int remove_mask) = 0;    // async
  virtual void SetProxyConfig(const std::string& json_encoded_settings) = 0;

  // Tab management.
  virtual void CreateTab(ChromeProxyDelegate* delegate,
                         const ExternalTabSettings& settings) = 0;
  virtual void ConnectTab(ChromeProxyDelegate* delegate, HWND hwnd,
                          uint64 cookie) = 0;
  virtual void BlockTab(uint64 cookie) = 0;

  // Tab related.
  virtual void Tab_PostMessage(int tab, const std::string& message,
                               const std::string& origin,
                               const std::string& target)  = 0;
  virtual void Tab_Reload(int tab) = 0;
  virtual void Tab_Stop(int tab) = 0;
  virtual void Tab_SaveAs(int tab) = 0;
  virtual void Tab_Print(int tab) = 0;
  virtual void Tab_Cut(int tab) = 0;
  virtual void Tab_Copy(int tab) = 0;
  virtual void Tab_Paste(int tab) = 0;
  virtual void Tab_SelectAll(int tab) = 0;
  virtual void Tab_Find(int tab, const string16& search_string,
      FindInPageDirection forward, FindInPageCase match_case,
      bool find_next) = 0;
  virtual void Tab_MenuCommand(int tab, int selected_command) = 0;

  // UI
  virtual void Tab_Zoom(int tab, content::PageZoom zoom_level) = 0;
  virtual void Tab_FontSize(int tab, enum AutomationPageFontSize font_size) = 0;
  virtual void Tab_SetInitialFocus(int tab,
      bool reverse, bool restore_focus_to_view) = 0;
  virtual void Tab_SetParentWindow(int tab) = 0;
  virtual void Tab_Resize(int tab) = 0;
  virtual void Tab_ProcessAccelerator(int tab, const MSG& msg) = 0;

  // Misc.
  virtual void Tab_OnHostMoved(int tab) = 0;
  virtual void Tab_RunUnloadHandlers(int tab) = 0;
  virtual void Tab_Navigate(int tab, const GURL& url, const GURL& referrer) = 0;
  virtual void Tab_OverrideEncoding(int tab, const char* encoding) = 0;

 protected:
  // Accessible by ChromeProxyFactory
  friend class ChromeProxyFactory;
  virtual ~ChromeProxy() {}
  virtual void Init(const ProxyParams& params) = 0;
  virtual int AddDelegate(ChromeProxyDelegate* delegate) = 0;
  virtual int RemoveDelegate(ChromeProxyDelegate* delegate) = 0;
};

// The object that uses ChromeProxy should implement ChromeProxyDelegate in
// order to get notified about important events/requests coming from the
// instance of Chromium.
// Allow only one delegate per tab, i.e. delegate can handle only a single tab.
// Note: all of the methods are invoked always in a background IPC thread.
class ChromeProxyDelegate : public IPC::Channel::Listener {
 public:
  enum DisconnectReason {
    CHROME_EXE_LAUNCH_FAILED,
    CHROME_EXE_LAUNCH_TIMEOUT,
    CHANNEL_ERROR
  };

  virtual void Connected(ChromeProxy* proxy) = 0;
  virtual void Disconnected() = 0;
  virtual void PeerLost(ChromeProxy* proxy, DisconnectReason reason) = 0;
  virtual int tab_handle() = 0;   // to avoid reverse lookup :)

  // Sync message responses.
  virtual void Completed_CreateTab(bool success, HWND chrome_wnd,
      HWND tab_window, int tab_handle, int session_id) = 0;
  virtual void Completed_ConnectToTab(bool success, HWND chrome_window,
      HWND tab_window, int tab_handle, int session_id) = 0;
  virtual void Completed_Navigate(bool success,
      enum AutomationMsg_NavigationResponseValues res) = 0;

 protected:
  ~ChromeProxyDelegate() {}
};

// a way to obtain a ChromeProxy implementation
struct ProxyParams {
  std::string profile;
  std::wstring extra_params;
  FilePath profile_path;
  base::TimeDelta timeout;
};

class ChromeProxyFactory {
 public:
  ChromeProxyFactory();
  ~ChromeProxyFactory();
  void GetProxy(ChromeProxyDelegate* delegate, const ProxyParams& params);
  bool ReleaseProxy(ChromeProxyDelegate* delegate, const std::string& profile);
 protected:
  virtual ChromeProxy* CreateProxy();
  typedef std::map<std::string, ChromeProxy*> ProxyMap;
  ProxyMap proxies_;
  base::Lock lock_;
};

#endif  // CHROME_FRAME_CFPROXY_H_
