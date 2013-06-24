// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_window_custom_bindings.h"

#include <string>

#include "chrome/common/extensions/extension_messages.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "chrome/renderer/extensions/scoped_persistent.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace extensions {

class DidCreateDocumentElementObserver : public content::RenderViewObserver {
 public:
  DidCreateDocumentElementObserver(
      content::RenderView* view, Dispatcher* dispatcher)
      : content::RenderViewObserver(view), dispatcher_(dispatcher) {
  }

  virtual void DidCreateDocumentElement(WebKit::WebFrame* frame) OVERRIDE {
    DCHECK(frame);
    DCHECK(dispatcher_);
    // Don't attempt to inject the titlebar into iframes.
    if (frame->parent())
      return;
    v8::HandleScope handle_scope;
    ChromeV8Context* v8_context =
        dispatcher_->v8_context_set().GetByV8Context(
            frame->mainWorldScriptContext());

    if (!v8_context)
      return;
    v8::Context::Scope context_scope(v8_context->v8_context());
    v8_context->module_system()->CallModuleMethod(
        "injectAppTitlebar", "didCreateDocumentElement");
  }

 private:
  Dispatcher* dispatcher_;
};

AppWindowCustomBindings::AppWindowCustomBindings(
    Dispatcher* dispatcher,
    ChromeV8Context* context) : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetView",
      base::Bind(&AppWindowCustomBindings::GetView,
                 base::Unretained(this)));
}

void AppWindowCustomBindings::GetView(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // TODO(jeremya): convert this to IDL nocompile to get validation, and turn
  // these argument checks into CHECK().
  if (args.Length() != 2)
    return;

  if (!args[0]->IsInt32())
    return;

  if (!args[1]->IsBoolean())
    return;

  int view_id = args[0]->Int32Value();

  bool inject_titlebar = args[1]->BooleanValue();

  if (view_id == MSG_ROUTING_NONE)
    return;

  content::RenderView* view = content::RenderView::FromRoutingID(view_id);
  if (!view)
    return;

  if (inject_titlebar)
    new DidCreateDocumentElementObserver(view, dispatcher());

  // TODO(jeremya): it doesn't really make sense to set the opener here, but we
  // need to make sure the security origin is set up before returning the DOM
  // reference. A better way to do this would be to have the browser pass the
  // opener through so opener_id is set in RenderViewImpl's constructor.
  content::RenderView* render_view = GetRenderView();
  if (!render_view)
    return;
  WebKit::WebFrame* opener = render_view->GetWebView()->mainFrame();
  WebKit::WebFrame* frame = view->GetWebView()->mainFrame();
  frame->setOpener(opener);
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_ResumeRequests(view->GetRoutingID()));

  v8::Local<v8::Value> window = frame->mainWorldScriptContext()->Global();
  args.GetReturnValue().Set(window);
}

}  // namespace extensions
