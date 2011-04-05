// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_helper.h"

#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/extensions/user_script_idle_scheduler.h"
#include "chrome/renderer/extensions/user_script_slave.h"

using WebKit::WebFrame;
using WebKit::WebDataSource;

ExtensionHelper::ExtensionHelper(RenderView* render_view)
    : RenderViewObserver(render_view) {
}

ExtensionHelper::~ExtensionHelper() {
  DCHECK(user_script_idle_schedulers_.empty());
}

bool ExtensionHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_Response, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnExtensionMessageInvoke)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionHelper::DidFinishDocumentLoad(WebFrame* frame) {
  ExtensionDispatcher::Get()->user_script_slave()->InjectScripts(
      frame, UserScript::DOCUMENT_END);
}

void ExtensionHelper::DidCreateDocumentElement(WebFrame* frame) {
  ExtensionDispatcher::Get()->user_script_slave()->InjectScripts(
      frame, UserScript::DOCUMENT_START);
}

void ExtensionHelper::FrameDetached(WebFrame* frame) {
  // This could be called before DidCreateDataSource, in which case the frame
  // won't be in the set.
  user_script_idle_schedulers_.erase(frame);
}

void ExtensionHelper::DidCreateDataSource(WebFrame* frame, WebDataSource* ds) {
  // Check first if we created a scheduler for the frame, since this function
  // gets called for navigations within the document.
  if (user_script_idle_schedulers_.count(frame))
    return;

  new UserScriptIdleScheduler(render_view(), frame);
  user_script_idle_schedulers_.insert(frame);
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
