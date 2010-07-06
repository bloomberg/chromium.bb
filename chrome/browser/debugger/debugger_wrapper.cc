// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/debugger_wrapper.h"

#include "chrome/browser/debugger/devtools_http_protocol_handler.h"
#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/debugger/devtools_protocol_handler.h"
#include "chrome/browser/debugger/devtools_remote_service.h"
#include "chrome/browser/debugger/extension_ports_remote_service.h"

DebuggerWrapper::DebuggerWrapper(int port, bool useHttp) {
  if (port > 0) {
    if (useHttp) {
      http_handler_ = new DevToolsHttpProtocolHandler(port);
      http_handler_->Start();
    } else {
      proto_handler_ = new DevToolsProtocolHandler(port);
      proto_handler_->RegisterDestination(
          new DevToolsRemoteService(proto_handler_),
          DevToolsRemoteService::kToolName);
      proto_handler_->RegisterDestination(
          new DebuggerRemoteService(proto_handler_),
          DebuggerRemoteService::kToolName);
      proto_handler_->RegisterDestination(
          new ExtensionPortsRemoteService(proto_handler_),
          ExtensionPortsRemoteService::kToolName);
      proto_handler_->Start();
    }
  }
}

DebuggerWrapper::~DebuggerWrapper() {
  if (proto_handler_.get() != NULL)
    proto_handler_->Stop();

  if (http_handler_.get() != NULL)
    http_handler_->Stop();
}
