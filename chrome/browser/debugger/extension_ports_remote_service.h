// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ExtensionsPorts service: wires extension message ports through the
// devtools remote protocol, allowing an external client program to
// exchange messages with Chrome extensions.

#ifndef CHROME_BROWSER_DEBUGGER_EXTENSION_PORTS_REMOTE_SERVICE_H_
#define CHROME_BROWSER_DEBUGGER_EXTENSION_PORTS_REMOTE_SERVICE_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/debugger/devtools_remote.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "ipc/ipc_message.h"

class DevToolsProtocolHandler;
class DevToolsRemoteMessage;
class GURL;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

class ExtensionPortsRemoteService : public DevToolsRemoteListener,
                                    public IPC::Message::Sender {
 public:
  // Specifies a tool name ("ExtensionPorts") handled by this class.
  static const char kToolName[];

  // |delegate| (never NULL) is the protocol handler instance which
  // dispatches messages to this service.
  // The ownership of |delegate| is NOT transferred to this class.
  explicit ExtensionPortsRemoteService(DevToolsProtocolHandler* delegate);

  // DevToolsRemoteListener methods:

  // Processes |message| from the external client (where the tool is
  // "ExtensionPorts").
  virtual void HandleMessage(const DevToolsRemoteMessage& message) OVERRIDE;

  // Gets invoked on the external client socket connection loss.
  // Closes open message ports.
  virtual void OnConnectionLost() OVERRIDE;

  // IPC::Message::Sender methods:

  // This is the callback through which the ExtensionMessageService
  // passes us messages from extensions as well as disconnect events.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

 private:
  // Operation result returned in the "result" field in messages sent
  // to the external client.
  typedef enum {
    RESULT_OK = 0,
    RESULT_UNKNOWN_COMMAND,
    RESULT_NO_SERVICE,
    RESULT_PARAMETER_ERROR,
    RESULT_UNKNOWN_PORT,
    RESULT_TAB_NOT_FOUND,
    RESULT_CONNECT_FAILED,  // probably extension ID not found.
  } Result;

  virtual ~ExtensionPortsRemoteService();

  // Sends a JSON message with the |response| to the external client.
  // |tool| and |destination| are used as the respective header values.
  void SendResponse(const base::Value& response,
                    const std::string& tool,
                    const std::string& destination);

  // Handles requests from ExtensionMessageService to invoke specific
  // JavaScript functions. Currently we only handle the "disconnect" function.
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& function_name,
                                const base::ListValue& args,
                                const GURL& event_url);
  // Handles a message sent from an extension through the
  // ExtensionMessageService, to be passed to the external client.
  void OnDeliverMessage(int port_id, const std::string& message);
  // Handles a disconnect event sent from the ExtensionMessageService.
  void OnExtensionPortDisconnected(int port_id);

  // Implementation for the commands we can receive from the external client.
  // Opens a channel to an extension.
  void ConnectCommand(base::DictionaryValue* content,
                      base::DictionaryValue* response);
  // Disconnects a message port.
  void DisconnectCommand(int port_id, base::DictionaryValue* response);
  // Sends a message to an extension through an established message port.
  void PostMessageCommand(int port_id,
                          base::DictionaryValue* content,
                          base::DictionaryValue* response);

  // The delegate is used to send responses and events back to the
  // external client, and to resolve tab IDs.
  DevToolsProtocolHandler* delegate_;

  // Set of message port IDs we successfully opened.
  typedef std::set<int> PortIdSet;
  PortIdSet openPortIds_;

  scoped_refptr<ExtensionMessageService> service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPortsRemoteService);
};

#endif  // CHROME_BROWSER_DEBUGGER_EXTENSION_PORTS_REMOTE_SERVICE_H_
