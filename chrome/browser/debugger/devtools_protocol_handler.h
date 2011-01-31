// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_PROTOCOL_HANDLER_H_
#pragma once

#include <string>

#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/debugger/devtools_remote.h"
#include "net/base/listen_socket.h"

class InspectableTabProxy;
class DevToolsRemoteListenSocket;
class DevToolsRemoteMessage;

// Dispatches DevToolsRemoteMessages to their appropriate handlers (Tools)
// based on the "Tool" message header value.
class DevToolsProtocolHandler
    : public DevToolsRemoteListener,
      public OutboundSocketDelegate {
 public:
  typedef base::hash_map< std::string, scoped_refptr<DevToolsRemoteListener> >
      ToolToListenerMap;

  static scoped_refptr<DevToolsProtocolHandler> Start(int port);

  // Called from the main thread in order to stop protocol handler.
  // Will schedule tear down task on IO thread.
  void Stop();

  // Registers a |listener| to handle messages for a certain |tool_name| Tool.
  // |listener| is the new message handler to register.
  //     As DevToolsRemoteListener inherits base::RefCountedThreadSafe,
  //     you should have no problems with ownership and destruction.
  // |tool_name| is the name of the Tool to associate the listener with.
  void RegisterDestination(DevToolsRemoteListener* listener,
                           const std::string& tool_name);

  // Unregisters a |listener| so that it will no longer handle messages
  // directed to the specified |tool_name| tool.
  void UnregisterDestination(DevToolsRemoteListener* listener,
                             const std::string& tool_name);

  InspectableTabProxy* inspectable_tab_proxy() {
    return inspectable_tab_proxy_.get();
  }

  // DevToolsRemoteListener interface
  virtual void HandleMessage(const DevToolsRemoteMessage& message);
  virtual void OnAcceptConnection(ListenSocket *connection);
  virtual void OnConnectionLost();

  // OutboundSocketDelegate interface
  virtual void Send(const DevToolsRemoteMessage& message);

 private:
  explicit DevToolsProtocolHandler(int port);
  virtual ~DevToolsProtocolHandler();
  void Start();

  void Init();
  void Teardown();
  int port_;
  ToolToListenerMap tool_to_listener_map_;
  scoped_refptr<ListenSocket> connection_;
  scoped_refptr<DevToolsRemoteListenSocket> server_;
  scoped_ptr<InspectableTabProxy> inspectable_tab_proxy_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsProtocolHandler);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_PROTOCOL_HANDLER_H_
