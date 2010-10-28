// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_CUSTOM_SYNC_CALL_CONTEXT_H_
#define CHROME_FRAME_CUSTOM_SYNC_CALL_CONTEXT_H_

#include <vector>
#include "base/ref_counted.h"
#include "base/waitable_event.h"
#include "chrome_frame/sync_msg_reply_dispatcher.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "ipc/ipc_sync_message.h"

// TODO(ananta)
// Move the implementations of these classes to the source file.

// Class that maintains context during the async load/install extension
// operation.  When done, InstallExtensionComplete is posted back to the UI
// thread so that the users of ChromeFrameAutomationClient can be notified.
class InstallExtensionContext
    : public SyncMessageReplyDispatcher::SyncMessageCallContext {
 public:
  typedef Tuple1<AutomationMsg_ExtensionResponseValues> output_type;

  InstallExtensionContext(ChromeFrameAutomationClient* client,
      const FilePath& crx_path, void* user_data) : client_(client),
      crx_path_(crx_path), user_data_(user_data) {
  }

  ~InstallExtensionContext() {
  }

  void Completed(AutomationMsg_ExtensionResponseValues res) {
    client_->PostTask(FROM_HERE, NewRunnableMethod(client_.get(),
        &ChromeFrameAutomationClient::InstallExtensionComplete, crx_path_,
        user_data_, res));
  }

 private:
  scoped_refptr<ChromeFrameAutomationClient> client_;
  FilePath crx_path_;
  void* user_data_;
};

// Class that maintains context during the async retrieval of fetching the
// list of enabled extensions.  When done, GetEnabledExtensionsComplete is
// posted back to the UI thread so that the users of
// ChromeFrameAutomationClient can be notified.
class GetEnabledExtensionsContext
    : public SyncMessageReplyDispatcher::SyncMessageCallContext {
 public:
  typedef Tuple1<std::vector<FilePath> > output_type;

  GetEnabledExtensionsContext(
      ChromeFrameAutomationClient* client, void* user_data) : client_(client),
          user_data_(user_data) {
    extension_directories_ = new std::vector<FilePath>();
  }

  ~GetEnabledExtensionsContext() {
    // ChromeFrameAutomationClient::GetEnabledExtensionsComplete takes
    // ownership of extension_directories_.
  }

  std::vector<FilePath>* extension_directories() {
    return extension_directories_;
  }

  void Completed(
      std::vector<FilePath> result) {
    (*extension_directories_) = result;
    client_->PostTask(FROM_HERE, NewRunnableMethod(client_.get(),
      &ChromeFrameAutomationClient::GetEnabledExtensionsComplete,
      user_data_, extension_directories_));
  }

 private:
  scoped_refptr<ChromeFrameAutomationClient> client_;
  std::vector<FilePath>* extension_directories_;
  void* user_data_;
};

// Class that maintains contextual information for the create and connect
// external tab operations.
class CreateExternalTabContext
    : public SyncMessageReplyDispatcher::SyncMessageCallContext {
 public:
  typedef Tuple3<HWND, HWND, int> output_type;
  explicit CreateExternalTabContext(ChromeFrameAutomationClient* client)
      : client_(client) {
  }

  void Completed(HWND chrome_window, HWND tab_window, int tab_handle) {
    AutomationLaunchResult launch_result =
        client_->CreateExternalTabComplete(chrome_window, tab_window,
                                           tab_handle);
    client_->PostTask(FROM_HERE,
                      NewRunnableMethod(
                          client_.get(),
                          &ChromeFrameAutomationClient::InitializeComplete,
                          launch_result));
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
  explicit UnloadContext(base::WaitableEvent* unload_done, bool* should_unload)
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


