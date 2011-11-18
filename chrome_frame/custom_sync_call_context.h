// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CUSTOM_SYNC_CALL_CONTEXT_H_
#define CHROME_FRAME_CUSTOM_SYNC_CALL_CONTEXT_H_

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome_frame/sync_msg_reply_dispatcher.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "ipc/ipc_sync_message.h"

// TODO(ananta)
// Move the implementations of these classes to the source file.

// Class that maintains contextual information for the create and connect
// external tab operations.
class CreateExternalTabContext
    : public SyncMessageReplyDispatcher::SyncMessageCallContext {
 public:
  typedef Tuple4<HWND, HWND, int, int> output_type;
  explicit CreateExternalTabContext(ChromeFrameAutomationClient* client)
      : client_(client) {
  }

  void Completed(HWND chrome_window, HWND tab_window, int tab_handle,
                 int session_id) {
    AutomationLaunchResult launch_result =
        client_->CreateExternalTabComplete(chrome_window, tab_window,
                                           tab_handle, session_id);
    client_->PostTask(
        FROM_HERE, base::Bind(&ChromeFrameAutomationClient::InitializeComplete,
                              client_.get(), launch_result));
  }

 private:
  scoped_refptr<ChromeFrameAutomationClient> client_;
};

// This class maintains context information for the BeginNavigate operations
// pertaining to the external tab.
class BeginNavigateContext
    : public SyncMessageReplyDispatcher::SyncMessageCallContext {
 public:
  explicit BeginNavigateContext(ChromeFrameAutomationClient* client)
      : client_(client) {}

  typedef Tuple1<AutomationMsg_NavigationResponseValues> output_type;

  void Completed(AutomationMsg_NavigationResponseValues response) {
    client_->BeginNavigateCompleted(response);
  }

 private:
  scoped_refptr<ChromeFrameAutomationClient> client_;
};

// Class that maintains contextual information for the unload operation, i.e.
// when the user attempts to navigate away from a page rendered in ChromeFrame.
class UnloadContext
    : public SyncMessageReplyDispatcher::SyncMessageCallContext {
 public:
  typedef Tuple1<bool> output_type;
  UnloadContext(base::WaitableEvent* unload_done, bool* should_unload)
      : should_unload_(should_unload),
        unload_done_(unload_done) {
  }

  void Completed(bool should_unload) {
    *should_unload_ = should_unload;
    unload_done_->Signal();
    should_unload_ = NULL;
    unload_done_ = NULL;
    // This object will be destroyed after this. Cannot access any members
    // on returning from this function.
  }

 private:
  base::WaitableEvent* unload_done_;
  bool* should_unload_;
};

#endif  // CHROME_FRAME_CUSTOM_SYNC_CALL_CONTEXT_H_
