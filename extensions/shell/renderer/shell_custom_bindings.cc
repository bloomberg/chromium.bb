// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/renderer/shell_custom_bindings.h"

#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace extensions {

ShellCustomBindings::ShellCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetView",
      base::Bind(&ShellCustomBindings::GetView, base::Unretained(this)));
}

void ShellCustomBindings::GetView(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 1 || !args[0]->IsInt32())
    return;

  int view_id = args[0]->Int32Value();
  if (view_id == MSG_ROUTING_NONE)
    return;

  content::RenderView* view = content::RenderView::FromRoutingID(view_id);
  if (!view)
    return;

  // Set the opener so we have a security origin set up before returning the DOM
  // reference.
  content::RenderView* render_view = context()->GetRenderView();
  if (!render_view)
    return;
  blink::WebFrame* opener = render_view->GetWebView()->mainFrame();
  blink::WebFrame* frame = view->GetWebView()->mainFrame();
  frame->setOpener(opener);

  // Resume resource requests.
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_ResumeRequests(view->GetRoutingID()));

  // Return the script context.
  v8::Local<v8::Value> window = frame->mainWorldScriptContext()->Global();
  args.GetReturnValue().Set(window);
}

}  // namespace extensions
