// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_helper.h"

#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/renderer/api/automation/automation_api_helper.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using content::ConsoleMessageLevel;
using blink::WebConsoleMessage;
using blink::WebDataSource;
using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebURLRequest;
using blink::WebView;

namespace extensions {

ExtensionHelper::ExtensionHelper(content::RenderView* render_view,
                                 Dispatcher* dispatcher)
    : content::RenderViewObserver(render_view),
      content::RenderViewObserverTracker<ExtensionHelper>(render_view),
      dispatcher_(dispatcher) {
  // Lifecycle managed by RenderViewObserver.
  new AutomationApiHelper(render_view);
}

ExtensionHelper::~ExtensionHelper() {
}

bool ExtensionHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionHelper, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_MessageInvoke, OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ExtensionMsg_SetFrameName, OnSetFrameName)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AddMessageToConsole,
                        OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ExtensionMsg_AppWindowClosed,
                        OnAppWindowClosed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionHelper::DidCreateDocumentElement(WebLocalFrame* frame) {
  dispatcher_->DidCreateDocumentElement(frame);
}

void ExtensionHelper::DraggableRegionsChanged(blink::WebFrame* frame) {
  blink::WebVector<blink::WebDraggableRegion> webregions =
      frame->document().draggableRegions();
  std::vector<DraggableRegion> regions;
  for (size_t i = 0; i < webregions.size(); ++i) {
    DraggableRegion region;
    region.bounds = webregions[i].bounds;
    region.draggable = webregions[i].draggable;
    regions.push_back(region);
  }
  Send(new ExtensionHostMsg_UpdateDraggableRegions(routing_id(), regions));
}

void ExtensionHelper::DidMatchCSS(
    blink::WebLocalFrame* frame,
    const blink::WebVector<blink::WebString>& newly_matching_selectors,
    const blink::WebVector<blink::WebString>& stopped_matching_selectors) {
  dispatcher_->DidMatchCSS(
      frame, newly_matching_selectors, stopped_matching_selectors);
}

void ExtensionHelper::OnExtensionMessageInvoke(const std::string& extension_id,
                                               const std::string& module_name,
                                               const std::string& function_name,
                                               const base::ListValue& args,
                                               bool user_gesture) {
  dispatcher_->InvokeModuleSystemMethod(
      render_view(), extension_id, module_name, function_name, args,
      user_gesture);
}

void ExtensionHelper::OnSetFrameName(const std::string& name) {
  blink::WebView* web_view = render_view()->GetWebView();
  if (web_view)
    web_view->mainFrame()->setName(blink::WebString::fromUTF8(name));
}

void ExtensionHelper::OnAddMessageToConsole(ConsoleMessageLevel level,
                                            const std::string& message) {
  console::AddMessage(render_view(), level, message);
}

void ExtensionHelper::OnAppWindowClosed() {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Context> v8_context =
      render_view()->GetWebView()->mainFrame()->mainWorldScriptContext();
  ScriptContext* script_context =
      dispatcher_->script_context_set().GetByV8Context(v8_context);
  if (!script_context)
    return;
  script_context->module_system()->CallModuleMethod("app.window",
                                                    "onAppWindowClosed");
}

}  // namespace extensions
