// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/custom_sync_call_context.h"

#include "base/bind.h"

CreateExternalTabContext::CreateExternalTabContext(
    ChromeFrameAutomationClient* client)
    : client_(client) {
}

void CreateExternalTabContext::Completed(
    HWND chrome_window, HWND tab_window, int tab_handle, int session_id) {
  AutomationLaunchResult launch_result =
      client_->CreateExternalTabComplete(chrome_window, tab_window,
                                         tab_handle, session_id);
  client_->PostTask(
      FROM_HERE, base::Bind(&ChromeFrameAutomationClient::InitializeComplete,
                            client_.get(), launch_result));
}

BeginNavigateContext::BeginNavigateContext(ChromeFrameAutomationClient* client)
    : client_(client) {
}

void BeginNavigateContext::Completed(
    AutomationMsg_NavigationResponseValues response) {
  client_->BeginNavigateCompleted(response);
}

UnloadContext::UnloadContext(
    base::WaitableEvent* unload_done, bool* should_unload)
    : should_unload_(should_unload),
      unload_done_(unload_done) {
}

void UnloadContext::Completed(bool should_unload) {
  *should_unload_ = should_unload;
  unload_done_->Signal();
  should_unload_ = NULL;
  unload_done_ = NULL;
  // This object will be destroyed after this. Cannot access any members
  // on returning from this function.
}
