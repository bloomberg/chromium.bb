// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_protocol_handler.h"

#include "base/logging.h"
#include "chrome/browser/debugger/inspectable_tab_proxy.h"
#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/debugger/devtools_remote_message.h"
#include "chrome/browser/debugger/devtools_remote_listen_socket.h"
#include "chrome/browser/debugger/devtools_remote_service.h"
#include "chrome/browser/debugger/extension_ports_remote_service.h"
#include "content/browser/browser_thread.h"

// static
scoped_refptr<DevToolsProtocolHandler> DevToolsProtocolHandler::Start(
    int port) {
  scoped_refptr<DevToolsProtocolHandler> proto_handler =
      new DevToolsProtocolHandler(port);
  proto_handler->RegisterDestination(
      new DevToolsRemoteService(proto_handler),
      DevToolsRemoteService::kToolName);
  proto_handler->RegisterDestination(
      new DebuggerRemoteService(proto_handler),
      DebuggerRemoteService::kToolName);
  proto_handler->RegisterDestination(
      new ExtensionPortsRemoteService(proto_handler),
      ExtensionPortsRemoteService::kToolName);
  proto_handler->Start();
  return proto_handler;
}

DevToolsProtocolHandler::DevToolsProtocolHandler(int port)
    : port_(port),
      connection_(NULL),
      server_(NULL) {
  inspectable_tab_proxy_.reset(new InspectableTabProxy);
}

DevToolsProtocolHandler::~DevToolsProtocolHandler() {
  // Stop() must be called prior to this being called
  DCHECK(server_.get() == NULL);
  DCHECK(connection_.get() == NULL);
}

void DevToolsProtocolHandler::Start() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsProtocolHandler::Init));
}

void DevToolsProtocolHandler::Init() {
  server_ = DevToolsRemoteListenSocket::Listen(
      "127.0.0.1", port_, this);
}

void DevToolsProtocolHandler::Stop() {
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &DevToolsProtocolHandler::Teardown));
  tool_to_listener_map_.clear();  // Releases all scoped_refptr's to listeners
}

// Run in I/O thread
void DevToolsProtocolHandler::Teardown() {
  connection_ = NULL;
  server_ = NULL;
}

void DevToolsProtocolHandler::RegisterDestination(
    DevToolsRemoteListener* listener,
    const std::string& tool_name) {
  DCHECK(tool_to_listener_map_.find(tool_name) == tool_to_listener_map_.end());
  tool_to_listener_map_.insert(std::make_pair(tool_name, listener));
}

void DevToolsProtocolHandler::UnregisterDestination(
    DevToolsRemoteListener* listener,
    const std::string& tool_name) {
  DCHECK(tool_to_listener_map_.find(tool_name) != tool_to_listener_map_.end());
  DCHECK(tool_to_listener_map_.find(tool_name)->second == listener);
  tool_to_listener_map_.erase(tool_name);
}

void DevToolsProtocolHandler::HandleMessage(
    const DevToolsRemoteMessage& message) {
  std::string tool = message.GetHeaderWithEmptyDefault(
      DevToolsRemoteMessageHeaders::kTool);
  ToolToListenerMap::const_iterator it = tool_to_listener_map_.find(tool);
  if (it == tool_to_listener_map_.end()) {
    NOTREACHED();  // an unsupported tool, bail out
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          it->second.get(), &DevToolsRemoteListener::HandleMessage, message));
}

void DevToolsProtocolHandler::Send(const DevToolsRemoteMessage& message) {
  if (connection_ != NULL) {
    connection_->Send(message.ToString());
  }
}

void DevToolsProtocolHandler::OnAcceptConnection(ListenSocket *connection) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  connection_ = connection;
}

void DevToolsProtocolHandler::OnConnectionLost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  connection_ = NULL;
  for (ToolToListenerMap::const_iterator it = tool_to_listener_map_.begin(),
       end = tool_to_listener_map_.end();
       it != end;
       ++it) {
    BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          it->second.get(), &DevToolsRemoteListener::OnConnectionLost));
  }
}
