// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/principals_message_filter.h"

#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"

PrincipalsMessageFilter::PrincipalsMessageFilter(int render_process_id)
    : BrowserMessageFilter(ChromeMsgStart),
      render_process_id_(render_process_id) {}

PrincipalsMessageFilter::~PrincipalsMessageFilter(){}

void PrincipalsMessageFilter::OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) {
  // GetManagedAccounts message is synchronous, it must be handled in the IO
  // thread, so no need to change thread, otherwise switch to UI thread
  if (message.type() == ChromeViewHostMsg_ShowBrowserAccountManagementUI::ID)
    *thread = content::BrowserThread::UI;
}

bool PrincipalsMessageFilter::OnMessageReceived(const IPC::Message& message) {
 bool handled = true;
 IPC_BEGIN_MESSAGE_MAP(PrincipalsMessageFilter, message)
     IPC_MESSAGE_HANDLER(
         ChromeViewHostMsg_GetManagedAccounts, OnMsgGetManagedAccounts)
     IPC_MESSAGE_HANDLER(
         ChromeViewHostMsg_ShowBrowserAccountManagementUI,
         OnMsgShowBrowserAccountManagementUI)
     IPC_MESSAGE_UNHANDLED(handled = false)
 IPC_END_MESSAGE_MAP()
 return handled;
}



void PrincipalsMessageFilter::OnMsgShowBrowserAccountManagementUI(){
  // TODO(guohui)
}

void PrincipalsMessageFilter::OnMsgGetManagedAccounts(
    const GURL& url, std::vector<std::string>* managed_accounts) {
  // TODO(guohui)
}

