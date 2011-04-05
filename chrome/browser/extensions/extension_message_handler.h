// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_HANDLER_H_
#pragma once

#include "content/browser/tab_contents/tab_contents_observer.h"
#include "ipc/ipc_channel.h"

class ExtensionFunctionDispatcher;
class Profile;
struct ExtensionHostMsg_DomMessage_Params;

// Filters and dispatches extension-related IPC messages that arrive from
// renderer/extension processes.  This object is created for renderers and also
// ExtensionHost/BackgroundContents.  Contrast this with ExtensionTabHelper,
// which is only created for TabContents.
class ExtensionMessageHandler : public IPC::Channel::Listener {
 public:
  // |sender| is guaranteed to outlive this object.
  ExtensionMessageHandler(int child_id,
                          IPC::Message::Sender* sender,
                          Profile* profile);
  virtual ~ExtensionMessageHandler();

  // IPC::Channel::Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Returns true iff the message can be dispatched.
  bool CanDispatchRequest(int child_id,
                          int routing_id,
                          const ExtensionHostMsg_DomMessage_Params& params);

  void set_extension_function_dispatcher(ExtensionFunctionDispatcher* e) {
    extension_function_dispatcher_ = e;
  }

 private:
  // Message handlers.
  void OnPostMessage(int port_id, const std::string& message);
  void OnRequest(const IPC::Message& message,
                 const ExtensionHostMsg_DomMessage_Params& params);

  // The child id of the corresponding process.  Can be 0.
  int child_id_;

  // Guaranteed to outlive this object.
  IPC::Message::Sender* sender_;
  ExtensionFunctionDispatcher* extension_function_dispatcher_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageHandler);
};

// A TabContentsObserver that forwards IPCs to ExtensionMessageHandler.
class ExtensionMessageObserver : public TabContentsObserver {
 public:
  explicit ExtensionMessageObserver(TabContents* tab_contents);
  ~ExtensionMessageObserver();

 private:
  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnRequest(const IPC::Message& message,
                 const ExtensionHostMsg_DomMessage_Params& params);

  ExtensionMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageObserver);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_HANDLER_H_
