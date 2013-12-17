// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/chrome_frame_delegate.h"

#include "chrome/common/automation_messages.h"

#define NO_CODE ((void)0)

bool ChromeFrameDelegateImpl::IsTabMessage(const IPC::Message& message) {
  bool is_tab_message = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeFrameDelegateImpl, message)
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestStart, NO_CODE)
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestRead, NO_CODE)
    IPC_MESSAGE_HANDLER_GENERIC(AutomationMsg_RequestEnd, NO_CODE)
    IPC_MESSAGE_UNHANDLED(is_tab_message = false);
  IPC_END_MESSAGE_MAP()

  return is_tab_message;
}

#undef NO_CODE

bool ChromeFrameDelegateImpl::OnMessageReceived(const IPC::Message& msg) {
  if (!IsValid()) {
    DLOG(WARNING) << __FUNCTION__
                  << " Msgs received for a NULL automation client instance";
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeFrameDelegateImpl, msg)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestStart, OnRequestStart)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestRead, OnRequestRead)
    IPC_MESSAGE_HANDLER(AutomationMsg_RequestEnd, OnRequestEnd)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}
