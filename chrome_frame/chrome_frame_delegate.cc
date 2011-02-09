// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_delegate.h"

#include "chrome/common/automation_messages.h"

bool ChromeFrameDelegateImpl::IsTabMessage(const IPC::Message& message) {
  bool is_tab_message = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeFrameDelegateImpl, message)
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_NavigationStateChanged, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_UpdateTargetUrl, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_HandleAccelerator, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_TabbedOut, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_OpenURL, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_NavigationFailed, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_DidNavigate, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_TabLoaded, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_ForwardMessageToExternalHost, )
    IPC_MESSAGE_HANDLER_GENERIC(
        AutomationMsg_ForwardContextMenuToExternalHost, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestStart, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestRead, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestEnd, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_DownloadRequestInHost, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_SetCookieAsync, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_AttachExternalTab, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestGoToHistoryEntryOffset, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_GetCookiesFromHost, )
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_CloseExternalTab, )
    IPC_MESSAGE_UNHANDLED(is_tab_message = false);
  IPC_END_MESSAGE_MAP()

  return is_tab_message;
}

bool ChromeFrameDelegateImpl::OnMessageReceived(const IPC::Message& msg) {
  if (!IsValid()) {
    DLOG(WARNING) << __FUNCTION__
                  << " Msgs received for a NULL automation client instance";
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeFrameDelegateImpl, msg)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationStateChanged,
                        OnNavigationStateChanged)
    IPC_MESSAGE_HANDLER(AutomationMsg_UpdateTargetUrl, OnUpdateTargetUrl)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleAccelerator,
                        OnAcceleratorPressed)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabbedOut, OnTabbedOut)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenURL, OnOpenURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationFailed, OnNavigationFailed)
    IPC_MESSAGE_HANDLER(AutomationMsg_DidNavigate, OnDidNavigate)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabLoaded, OnLoad)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardMessageToExternalHost,
                        OnMessageFromChromeFrame)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardContextMenuToExternalHost,
                        OnHandleContextMenu)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStart, OnRequestStart)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestRead, OnRequestRead)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadRequestInHost,
                        OnDownloadRequestInHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookieAsync, OnSetCookieAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_AttachExternalTab, OnAttachExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestGoToHistoryEntryOffset,
        OnGoToHistoryEntryOffset)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookiesFromHost, OnGetCookiesFromHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseExternalTab, OnCloseTab)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}
