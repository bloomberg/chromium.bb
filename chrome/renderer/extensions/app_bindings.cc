// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_bindings.h"

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

using blink::WebFrame;
using blink::WebLocalFrame;
using content::V8ValueConverter;

namespace extensions {

namespace {

bool IsCheckoutURL(const std::string& url_spec) {
  std::string checkout_url_prefix =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAppsCheckoutURL);
  if (checkout_url_prefix.empty())
    checkout_url_prefix = "https://checkout.google.com/";

  return StartsWithASCII(url_spec, checkout_url_prefix, false);
}

bool CheckAccessToAppDetails(WebFrame* frame, v8::Isolate* isolate) {
  if (!IsCheckoutURL(frame->document().url().spec())) {
    std::string error("Access denied for URL: ");
    error += frame->document().url().spec();
    isolate->ThrowException(v8::String::NewFromUtf8(isolate, error.c_str()));
    return false;
  }

  return true;
}

const char* kInvalidCallbackIdError = "Invalid callbackId";

}  // namespace

AppBindings::AppBindings(Dispatcher* dispatcher, ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      ChromeV8ExtensionHandler(context),
      dispatcher_(dispatcher) {
  RouteFunction("GetIsInstalled",
      base::Bind(&AppBindings::GetIsInstalled, base::Unretained(this)));
  RouteFunction("GetDetails",
      base::Bind(&AppBindings::GetDetails, base::Unretained(this)));
  RouteFunction("GetDetailsForFrame",
      base::Bind(&AppBindings::GetDetailsForFrame, base::Unretained(this)));
  RouteFunction("GetInstallState",
      base::Bind(&AppBindings::GetInstallState, base::Unretained(this)));
  RouteFunction("GetRunningState",
      base::Bind(&AppBindings::GetRunningState, base::Unretained(this)));
}

void AppBindings::GetIsInstalled(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  const Extension* extension = context()->extension();

  // TODO(aa): Why only hosted app?
  bool result = extension && extension->is_hosted_app() &&
      dispatcher_->IsExtensionActive(extension->id());
  args.GetReturnValue().Set(result);
}

void AppBindings::GetDetails(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(context()->web_frame());
  args.GetReturnValue().Set(GetDetailsForFrameImpl(context()->web_frame()));
}

void AppBindings::GetDetailsForFrame(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(context()->web_frame());
  if (!CheckAccessToAppDetails(context()->web_frame(), context()->isolate()))
    return;

  if (args.Length() < 0) {
    context()->isolate()->ThrowException(
        v8::String::NewFromUtf8(context()->isolate(), "Not enough arguments."));
    return;
  }

  if (!args[0]->IsObject()) {
    context()->isolate()->ThrowException(v8::String::NewFromUtf8(
        context()->isolate(), "Argument 0 must be an object."));
    return;
  }

  v8::Local<v8::Context> context =
      v8::Local<v8::Object>::Cast(args[0])->CreationContext();
  CHECK(!context.IsEmpty());

  WebLocalFrame* target_frame = WebLocalFrame::frameForContext(context);
  if (!target_frame) {
    console::Error(args.GetIsolate()->GetCallingContext(),
                   "Could not find frame for specified object.");
    return;
  }

  args.GetReturnValue().Set(GetDetailsForFrameImpl(target_frame));
}

v8::Handle<v8::Value> AppBindings::GetDetailsForFrameImpl(
    WebFrame* frame) {
  v8::Isolate* isolate = frame->mainWorldScriptContext()->GetIsolate();
  if (frame->document().securityOrigin().isUnique())
    return v8::Null(isolate);

  const Extension* extension =
      dispatcher_->extensions()->GetExtensionOrAppByURL(
          frame->document().url());

  if (!extension)
    return v8::Null(isolate);

  scoped_ptr<base::DictionaryValue> manifest_copy(
      extension->manifest()->value()->DeepCopy());
  manifest_copy->SetString("id", extension->id());
  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  return converter->ToV8Value(manifest_copy.get(),
                              frame->mainWorldScriptContext());
}

void AppBindings::GetInstallState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Get the callbackId.
  int callback_id = 0;
  if (args.Length() == 1) {
    if (!args[0]->IsInt32()) {
      context()->isolate()->ThrowException(v8::String::NewFromUtf8(
          context()->isolate(), kInvalidCallbackIdError));
      return;
    }
    callback_id = args[0]->Int32Value();
  }

  content::RenderView* render_view = context()->GetRenderView();
  CHECK(render_view);

  Send(new ExtensionHostMsg_GetAppInstallState(
      render_view->GetRoutingID(), context()->web_frame()->document().url(),
      GetRoutingID(), callback_id));
}

void AppBindings::GetRunningState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // To distinguish between ready_to_run and cannot_run states, we need the top
  // level frame.
  const WebFrame* parent_frame = context()->web_frame();
  while (parent_frame->parent())
    parent_frame = parent_frame->parent();

  const ExtensionSet* extensions = dispatcher_->extensions();

  // The app associated with the top level frame.
  const Extension* parent_app = extensions->GetHostedAppByURL(
      parent_frame->document().url());

  // The app associated with this frame.
  const Extension* this_app = extensions->GetHostedAppByURL(
      context()->web_frame()->document().url());

  if (!this_app || !parent_app) {
    args.GetReturnValue().Set(v8::String::NewFromUtf8(
        context()->isolate(), extension_misc::kAppStateCannotRun));
    return;
  }

  const char* state = NULL;
  if (dispatcher_->IsExtensionActive(parent_app->id())) {
    if (parent_app == this_app)
      state = extension_misc::kAppStateRunning;
    else
      state = extension_misc::kAppStateCannotRun;
  } else if (parent_app == this_app) {
    state = extension_misc::kAppStateReadyToRun;
  } else {
    state = extension_misc::kAppStateCannotRun;
  }

  args.GetReturnValue()
      .Set(v8::String::NewFromUtf8(context()->isolate(), state));
}

bool AppBindings::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AppBindings, message)
    IPC_MESSAGE_HANDLER(ExtensionMsg_GetAppInstallStateResponse,
                        OnAppInstallStateResponse)
    IPC_MESSAGE_UNHANDLED(CHECK(false) << "Unhandled IPC message")
  IPC_END_MESSAGE_MAP()
  return true;
}

void AppBindings::OnAppInstallStateResponse(
    const std::string& state, int callback_id) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(context()->v8_context());
  v8::Handle<v8::Value> argv[] = {
    v8::String::NewFromUtf8(isolate, state.c_str()),
    v8::Integer::New(isolate, callback_id)
  };
  context()->module_system()->CallModuleMethod(
      "app", "onInstallStateResponse", arraysize(argv), argv);
}

}  // namespace extensions
