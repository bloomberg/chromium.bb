// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/app_bindings.h"

#include <memory>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/console.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "v8/include/v8.h"

using blink::WebFrame;
using content::V8ValueConverter;

namespace extensions {

namespace {

const char kInvalidCallbackIdError[] = "Invalid callbackId";

}  // namespace

AppBindings::AppBindings(Dispatcher* dispatcher, ScriptContext* context)
    : ObjectBackedNativeHandler(context),
      dispatcher_(dispatcher) {
  RouteFunction(
      "GetIsInstalled", "app.getIsInstalled",
      base::Bind(&AppBindings::GetIsInstalled, base::Unretained(this)));
  RouteFunction("GetDetails", "app.getDetails",
                base::Bind(&AppBindings::GetDetails, base::Unretained(this)));
  RouteFunction(
      "GetInstallState", "app.installState",
      base::Bind(&AppBindings::GetInstallState, base::Unretained(this)));
  RouteFunction(
      "GetRunningState", "app.runningState",
      base::Bind(&AppBindings::GetRunningState, base::Unretained(this)));
}

AppBindings::~AppBindings() {
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
  blink::WebLocalFrame* web_frame = context()->web_frame();
  CHECK(web_frame);
  args.GetReturnValue().Set(GetDetailsImpl(web_frame));
}

v8::Local<v8::Value> AppBindings::GetDetailsImpl(blink::WebLocalFrame* frame) {
  v8::Isolate* isolate = frame->mainWorldScriptContext()->GetIsolate();
  if (frame->document().getSecurityOrigin().isUnique())
    return v8::Null(isolate);

  const Extension* extension =
      RendererExtensionRegistry::Get()->GetExtensionOrAppByURL(
          frame->document().url());

  if (!extension)
    return v8::Null(isolate);

  std::unique_ptr<base::DictionaryValue> manifest_copy(
      extension->manifest()->value()->DeepCopy());
  manifest_copy->SetString("id", extension->id());
  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
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

  content::RenderFrame* render_frame = context()->GetRenderFrame();
  CHECK(render_frame);

  Send(new ExtensionHostMsg_GetAppInstallState(
      render_frame->GetRoutingID(), context()->web_frame()->document().url(),
      GetRoutingID(), callback_id));
}

void AppBindings::GetRunningState(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // To distinguish between ready_to_run and cannot_run states, we need the app
  // from the top frame.
  blink::WebSecurityOrigin top_frame_security_origin =
      context()->web_frame()->top()->getSecurityOrigin();
  const RendererExtensionRegistry* extensions =
      RendererExtensionRegistry::Get();

  // The app associated with the top level frame.
  const Extension* top_app = extensions->GetHostedAppByURL(
      GURL(top_frame_security_origin.toString().utf8()));

  // The app associated with this frame.
  const Extension* this_app = extensions->GetHostedAppByURL(
      context()->web_frame()->document().url());

  if (!this_app || !top_app) {
    args.GetReturnValue().Set(v8::String::NewFromUtf8(
        context()->isolate(), extension_misc::kAppStateCannotRun));
    return;
  }

  const char* state = nullptr;
  if (dispatcher_->IsExtensionActive(top_app->id())) {
    if (top_app == this_app)
      state = extension_misc::kAppStateRunning;
    else
      state = extension_misc::kAppStateCannotRun;
  } else if (top_app == this_app) {
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
  v8::Local<v8::Value> argv[] = {
    v8::String::NewFromUtf8(isolate, state.c_str()),
    v8::Integer::New(isolate, callback_id)
  };
  context()->module_system()->CallModuleMethod(
      "app", "onInstallStateResponse", arraysize(argv), argv);
}

}  // namespace extensions
