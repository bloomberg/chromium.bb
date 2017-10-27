// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_http_handler.h"
#include "content/browser/devtools/devtools_pipe_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/devtools_socket_factory.h"

namespace content {

// static
void DevToolsAgentHost::StartRemoteDebuggingServer(
    std::unique_ptr<DevToolsSocketFactory> server_socket_factory,
    const std::string& frontend_url,
    const base::FilePath& active_port_output_directory,
    const base::FilePath& debug_frontend_dir) {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  if (!manager->delegate())
    return;
  manager->SetHttpHandler(std::make_unique<DevToolsHttpHandler>(
      manager->delegate(), std::move(server_socket_factory), frontend_url,
      active_port_output_directory, debug_frontend_dir));
}

// static
void DevToolsAgentHost::StartRemoteDebuggingPipeHandler() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->SetPipeHandler(std::make_unique<DevToolsPipeHandler>());
}

// static
void DevToolsAgentHost::StopRemoteDebuggingServer() {
  DevToolsManager* manager = DevToolsManager::GetInstance();
  manager->SetHttpHandler(nullptr);
}

// static
DevToolsManager* DevToolsManager::GetInstance() {
  return base::Singleton<DevToolsManager>::get();
}

DevToolsManager::DevToolsManager()
    : delegate_(GetContentClient()->browser()->GetDevToolsManagerDelegate()) {
}

DevToolsManager::~DevToolsManager() {
}

void DevToolsManager::SetHttpHandler(
    std::unique_ptr<DevToolsHttpHandler> http_handler) {
  http_handler_ = std::move(http_handler);
}

void DevToolsManager::SetPipeHandler(
    std::unique_ptr<DevToolsPipeHandler> pipe_handler) {
  pipe_handler_ = std::move(pipe_handler);
}

}  // namespace content
