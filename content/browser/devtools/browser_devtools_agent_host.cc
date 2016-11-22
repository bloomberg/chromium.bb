// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/browser_devtools_agent_host.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
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
      tethering_task_runner_(tethering_task_runner),
      socket_callback_(socket_callback) {
  NotifyCreated();
}

BrowserDevToolsAgentHost::~BrowserDevToolsAgentHost() {
}

void BrowserDevToolsAgentHost::Attach() {
  io_handler_.reset(new protocol::IOHandler(GetIOContext()));
  io_handler_->Wire(session()->dispatcher());

  memory_handler_.reset(new protocol::MemoryHandler());
  memory_handler_->Wire(session()->dispatcher());

  system_info_handler_.reset(new protocol::SystemInfoHandler());
  system_info_handler_->Wire(session()->dispatcher());

  tethering_handler_.reset(new protocol::TetheringHandler(
      socket_callback_, tethering_task_runner_));
  tethering_handler_->Wire(session()->dispatcher());

  tracing_handler_.reset(new protocol::TracingHandler(
      protocol::TracingHandler::Browser,
      FrameTreeNode::kFrameTreeNodeInvalidId,
      GetIOContext()));
  tracing_handler_->Wire(session()->dispatcher());
}

void BrowserDevToolsAgentHost::Detach() {
  io_handler_.reset();
  memory_handler_.reset();
  system_info_handler_.reset();
  tethering_handler_.reset();
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
  session()->dispatcher()->dispatch(protocol::StringUtil::parseJSON(message));
  return true;
}

}  // content
