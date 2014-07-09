// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_window_custom_bindings.h"

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_observer.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/scoped_persistent.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "grit/renderer_resources.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

namespace extensions {

class DidCreateDocumentElementObserver : public content::RenderViewObserver {
 public:
  DidCreateDocumentElementObserver(content::RenderView* view,
                                   Dispatcher* dispatcher)
      : content::RenderViewObserver(view), dispatcher_(dispatcher) {}

  virtual void DidCreateDocumentElement(blink::WebLocalFrame* frame) OVERRIDE {
    DCHECK(frame);
    DCHECK(dispatcher_);
    // Don't attempt to inject the titlebar into iframes.
    if (frame->parent())
      return;
    v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
    ScriptContext* script_context =
        dispatcher_->script_context_set().GetByV8Context(
            frame->mainWorldScriptContext());

    if (!script_context)
      return;
    v8::Context::Scope context_scope(script_context->v8_context());
    script_context->module_system()->CallModuleMethod(
        "injectAppTitlebar", "didCreateDocumentElement");
  }

 private:
  Dispatcher* dispatcher_;
};

AppWindowCustomBindings::AppWindowCustomBindings(Dispatcher* dispatcher,
                                                 ScriptContext* context)
    : ObjectBackedNativeHandler(context), dispatcher_(dispatcher) {
  RouteFunction("GetView",
      base::Bind(&AppWindowCustomBindings::GetView,
                 base::Unretained(this)));

  RouteFunction("GetWindowControlsHtmlTemplate",
      base::Bind(&AppWindowCustomBindings::GetWindowControlsHtmlTemplate,
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
    new DidCreateDocumentElementObserver(view, dispatcher_);

  // TODO(jeremya): it doesn't really make sense to set the opener here, but we
  // need to make sure the security origin is set up before returning the DOM
  // reference. A better way to do this would be to have the browser pass the
  // opener through so opener_id is set in RenderViewImpl's constructor.
  content::RenderView* render_view = context()->GetRenderView();
  if (!render_view)
    return;
  blink::WebFrame* opener = render_view->GetWebView()->mainFrame();
  blink::WebFrame* frame = view->GetWebView()->mainFrame();
  frame->setOpener(opener);
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_ResumeRequests(view->GetRoutingID()));

  v8::Local<v8::Value> window = frame->mainWorldScriptContext()->Global();
  args.GetReturnValue().Set(window);
}

void AppWindowCustomBindings::GetWindowControlsHtmlTemplate(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 0);

  v8::Handle<v8::Value> result = v8::String::Empty(args.GetIsolate());
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAppWindowControls)) {
    base::StringValue value(
        ResourceBundle::GetSharedInstance()
            .GetRawDataResource(IDR_WINDOW_CONTROLS_TEMPLATE_HTML)
            .as_string());
    scoped_ptr<content::V8ValueConverter> converter(
        content::V8ValueConverter::create());
    result = converter->ToV8Value(&value, context()->v8_context());
  }
  args.GetReturnValue().Set(result);
}

}  // namespace extensions
