// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/browser_devtools_agent_host.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "content/browser/devtools/devtools_protocol_handler.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/memory_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/system_info_handler.h"
#include "content/browser/devtools/protocol/tethering_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/frame_tree_node.h"

namespace content {

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::CreateForBrowser(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback) {
  return new BrowserDevToolsAgentHost(tethering_task_runner, socket_callback);
}

BrowserDevToolsAgentHost::BrowserDevToolsAgentHost(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback)
    : DevToolsAgentHostImpl(base::GenerateGUID()),
      memory_handler_(new devtools::memory::MemoryHandler()),
      system_info_handler_(new devtools::system_info::SystemInfoHandler()),
      tethering_handler_(
          new devtools::tethering::TetheringHandler(socket_callback,
                                                    tethering_task_runner)),
      protocol_handler_(new DevToolsProtocolHandler(this)) {
  DevToolsProtocolDispatcher* dispatcher = protocol_handler_->dispatcher();
  dispatcher->SetMemoryHandler(memory_handler_.get());
  dispatcher->SetSystemInfoHandler(system_info_handler_.get());
  dispatcher->SetTetheringHandler(tethering_handler_.get());
  NotifyCreated();
}

BrowserDevToolsAgentHost::~BrowserDevToolsAgentHost() {
}

void BrowserDevToolsAgentHost::Attach() {
  session()->dispatcher()->setFallThroughForNotFound(true);
  io_handler_.reset(new protocol::IOHandler(GetIOContext()));
  io_handler_->Wire(session()->dispatcher());

  tracing_handler_.reset(new protocol::TracingHandler(
      protocol::TracingHandler::Browser,
      FrameTreeNode::kFrameTreeNodeInvalidId,
      GetIOContext()));
  tracing_handler_->Wire(session()->dispatcher());
}

void BrowserDevToolsAgentHost::Detach() {
  io_handler_.reset();
  tracing_handler_.reset();
}

std::string BrowserDevToolsAgentHost::GetType() {
  return kTypeBrowser;
}

std::string BrowserDevToolsAgentHost::GetTitle() {
  return "";
}

GURL BrowserDevToolsAgentHost::GetURL() {
  return GURL();
}

bool BrowserDevToolsAgentHost::Activate() {
  return false;
}

bool BrowserDevToolsAgentHost::Close() {
  return false;
}

void BrowserDevToolsAgentHost::Reload() {
}

bool BrowserDevToolsAgentHost::DispatchProtocolMessage(
    const std::string& message) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(message);
  std::unique_ptr<protocol::Value> protocolValue =
      protocol::toProtocolValue(value.get(), 1000);
  if (session()->dispatcher()->dispatch(std::move(protocolValue)) ==
      protocol::Response::kFallThrough) {
    protocol_handler_->HandleMessage(session()->session_id(), std::move(value));
  }
  return true;
}

}  // content
