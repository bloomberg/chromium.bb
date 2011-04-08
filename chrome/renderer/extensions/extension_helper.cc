// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_helper.h"

#include "base/lazy_instance.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/extensions/user_script_idle_scheduler.h"
#include "chrome/renderer/extensions/user_script_slave.h"
#include "content/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDataSource;
using WebKit::WebFrame;
using WebKit::WebView;

namespace {
// Keeps a mapping from the frame pointer to a UserScriptIdleScheduler object.
// We store this mapping per process, because a frame can jump from one
// document to another with adoptNode, and so having the object be a
// RenderViewObserver means it might miss some notifications after it moves.
typedef std::map<WebFrame*, UserScriptIdleScheduler*> SchedulerMap;
static base::LazyInstance<SchedulerMap> g_schedulers(base::LINKER_INITIALIZED);
}

ExtensionHelper::ExtensionHelper(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

ExtensionHelper::~ExtensionHelper() {
}

bool ExtensionHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Response, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_ExecuteCode, OnExecuteCode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionHelper::DidFinishDocumentLoad(WebFrame* frame) {
  ExtensionDispatcher::Get()->user_script_slave()->InjectScripts(
      frame, UserScript::DOCUMENT_END);

  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i != g_schedulers.Get().end())
    i->second->DidFinishDocumentLoad();
}

void ExtensionHelper::DidFinishLoad(WebKit::WebFrame* frame) {
  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i != g_schedulers.Get().end())
    i->second->DidFinishLoad();
}

void ExtensionHelper::DidCreateDocumentElement(WebFrame* frame) {
  ExtensionDispatcher::Get()->user_script_slave()->InjectScripts(
      frame, UserScript::DOCUMENT_START);
}

void ExtensionHelper::DidStartProvisionalLoad(WebKit::WebFrame* frame) {
  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i != g_schedulers.Get().end())
    i->second->DidStartProvisionalLoad();
}

void ExtensionHelper::FrameDetached(WebFrame* frame) {
  // This could be called before DidCreateDataSource, in which case the frame
  // won't be in the map.
  SchedulerMap::iterator i = g_schedulers.Get().find(frame);
  if (i == g_schedulers.Get().end())
    return;

  delete i->second;
  g_schedulers.Get().erase(i);
}

void ExtensionHelper::DidCreateDataSource(WebFrame* frame, WebDataSource* ds) {
  // Check first if we created a scheduler for the frame, since this function
  // gets called for navigations within the document.
  if (g_schedulers.Get().count(frame))
    return;

  g_schedulers.Get()[frame] = new UserScriptIdleScheduler(frame);
}

void ExtensionHelper::OnExtensionResponse(int request_id,
                                          bool success,
                                          const std::string& response,
                                          const std::string& error) {
  ExtensionProcessBindings::HandleResponse(
      request_id, success, response, error);
}

void ExtensionHelper::OnExtensionMessageInvoke(const std::string& extension_id,
                                               const std::string& function_name,
                                               const ListValue& args,
                                               const GURL& event_url) {
  RendererExtensionBindings::Invoke(
      extension_id, function_name, args, render_view(), event_url);
}

void ExtensionHelper::OnExecuteCode(
    const ExtensionMsg_ExecuteCode_Params& params) {
  WebView* webview = render_view()->webview();
  WebFrame* main_frame = webview->mainFrame();
  if (!main_frame) {
    Send(new ExtensionHostMsg_ExecuteCodeFinished(
        routing_id(), params.request_id, false, ""));
    return;
  }

  // chrome.tabs.executeScript() only supports execution in either the top frame
  // or all frames.  We handle both cases in the top frame.
  SchedulerMap::iterator i = g_schedulers.Get().find(main_frame);
  if (i != g_schedulers.Get().end())
    i->second->ExecuteCode(params);
}
