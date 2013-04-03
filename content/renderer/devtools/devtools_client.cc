// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/devtools_client.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/common/devtools_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFloatPoint.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "ui/base/ui_base_switches.h"
#include "webkit/appcache/appcache_interfaces.h"

using WebKit::WebDevToolsFrontend;
using WebKit::WebString;

namespace content {

namespace {

bool g_devtools_frontend_testing_enabled = false;

}  // namespace

DevToolsClient::DevToolsClient(RenderViewImpl* render_view)
    : RenderViewObserver(render_view) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  web_tools_frontend_.reset(
      WebDevToolsFrontend::create(
          render_view->webview(),
          this,
          ASCIIToUTF16(command_line.GetSwitchValueASCII(switches::kLang))));
  appcache::AddSupportedScheme(chrome::kChromeDevToolsScheme);
}

DevToolsClient::~DevToolsClient() {
}

bool DevToolsClient::OnMessageReceived(const IPC::Message& message) {
  DCHECK(RenderThreadImpl::current());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsClient, message)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DevToolsClient::sendMessageToBackend(const WebString& message)  {
  Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(routing_id(),
                                                       message.utf8()));
}

void DevToolsClient::activateWindow() {
  Send(new DevToolsHostMsg_ActivateWindow(routing_id()));
}

void DevToolsClient::changeAttachedWindowHeight(unsigned height) {
  Send(new DevToolsHostMsg_ChangeAttachedWindowHeight(routing_id(), height));
}

void DevToolsClient::closeWindow() {
  Send(new DevToolsHostMsg_CloseWindow(routing_id()));
}

void DevToolsClient::moveWindowBy(const WebKit::WebFloatPoint& offset) {
  Send(new DevToolsHostMsg_MoveWindow(routing_id(), offset.x, offset.y));
}

void DevToolsClient::requestSetDockSide(const WebKit::WebString& side) {
  Send(new DevToolsHostMsg_RequestSetDockSide(routing_id(), side.utf8()));
}

void DevToolsClient::openInNewTab(const WebKit::WebString& url) {
  Send(new DevToolsHostMsg_OpenInNewTab(routing_id(),
                                        url.utf8()));
}

void DevToolsClient::save(const WebKit::WebString& url,
                          const WebKit::WebString& content,
                          bool save_as) {
  Send(new DevToolsHostMsg_Save(routing_id(),
                                url.utf8(),
                                content.utf8(),
                                save_as));
}

void DevToolsClient::append(const WebKit::WebString& url,
                            const WebKit::WebString& content) {
  Send(new DevToolsHostMsg_Append(routing_id(),
                                  url.utf8(),
                                  content.utf8()));
}

void DevToolsClient::requestFileSystems() {
  Send(new DevToolsHostMsg_RequestFileSystems(routing_id()));
}

void DevToolsClient::addFileSystem() {
  Send(new DevToolsHostMsg_AddFileSystem(routing_id()));
}

void DevToolsClient::removeFileSystem(const WebString& fileSystemPath) {
  Send(new DevToolsHostMsg_RemoveFileSystem(routing_id(),
                                            fileSystemPath.utf8()));
}

bool DevToolsClient::isUnderTest() {
  return g_devtools_frontend_testing_enabled;
}

void DevToolsClient::OnDispatchOnInspectorFrontend(const std::string& message) {
  web_tools_frontend_->dispatchOnInspectorFrontend(
      WebString::fromUTF8(message));
}

// static
void DevToolsClient::EnableDevToolsFrontendTesting() {
  g_devtools_frontend_testing_enabled = true;
}

}  // namespace content
