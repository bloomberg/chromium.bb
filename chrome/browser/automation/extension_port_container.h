// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_EXTENSION_PORT_CONTAINER_H_
#define CHROME_BROWSER_AUTOMATION_EXTENSION_PORT_CONTAINER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"

class AutomationProvider;
class ExtensionMessageService;
class GURL;
class ListValue;
class RenderViewHost;

// This class represents an external port to an extension, opened
// through the automation interface.
class ExtensionPortContainer : public IPC::Message::Sender {
 public:

  // Intercepts and processes a message posted through the automation interface.
  // Returns true if the message was intercepted.
  static bool InterceptMessageFromExternalHost(const std::string& message,
                                               const std::string& origin,
                                               const std::string& target,
                                               AutomationProvider* automation,
                                               RenderViewHost *view_host,
                                               int tab_handle);

  ExtensionPortContainer(AutomationProvider* automation, int tab_handle);
  ~ExtensionPortContainer();

  int port_id() const { return port_id_; }
  void set_port_id(int port_id) { port_id_ = port_id; }

  // IPC implementation.
  virtual bool Send(IPC::Message* msg);

 private:
  // Posts a message to the external host.
  bool PostMessageToExternalPort(const std::string& message);
  // Posts a request response message to the external host.
  bool PostResponseToExternalPort(const std::string& message);

  // Forwards a message from the external port.
  void PostMessageFromExternalPort(const std::string& message);

  // Attempts to connect this instance to the extension id, sends
  // a response to the connecting party.
  // Returns true if the connection was successful.
  bool Connect(const std::string &extension_id,
               int process_id,
               int routing_id,
               int connection_id,
               const std::string& channel_name,
               const std::string& tab_json);

  // Sends a connect response to the external port.
  void SendConnectionResponse(int connection_id, int port_id);

  void OnExtensionMessageInvoke(const std::string& function_name,
                                const ListValue& args,
                                bool requires_incognito_access,
                                const GURL& event_url);
  void OnExtensionHandleMessage(const std::string& message, int source_port_id);
  void OnExtensionPortDisconnected(int source_port_id);

  // Our automation provider.
  AutomationProvider* automation_;

  // The extension message service.
  scoped_refptr<ExtensionMessageService> service_;

  // Our assigned port id.
  int port_id_;
  // Handle to our associated tab.
  int tab_handle_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPortContainer);
};

#endif  // CHROME_BROWSER_AUTOMATION_EXTENSION_PORT_CONTAINER_H_
