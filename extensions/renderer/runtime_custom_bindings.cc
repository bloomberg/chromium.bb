// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/runtime_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest.h"
#include "extensions/renderer/api_activity_logger.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using content::V8ValueConverter;

namespace extensions {

RuntimeCustomBindings::RuntimeCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "GetManifest",
      base::Bind(&RuntimeCustomBindings::GetManifest, base::Unretained(this)));
  RouteFunction("OpenChannelToExtension",
                base::Bind(&RuntimeCustomBindings::OpenChannelToExtension,
                           base::Unretained(this)));
  RouteFunction("OpenChannelToNativeApp",
                base::Bind(&RuntimeCustomBindings::OpenChannelToNativeApp,
                           base::Unretained(this)));
  RouteFunction("GetExtensionViews",
                base::Bind(&RuntimeCustomBindings::GetExtensionViews,
                           base::Unretained(this)));
}

RuntimeCustomBindings::~RuntimeCustomBindings() {
}

void RuntimeCustomBindings::OpenChannelToExtension(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = context()->GetRenderView();
  if (!renderview)
    return;

  // The Javascript code should validate/fill the arguments.
  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsString() && args[1]->IsString() && args[2]->IsBoolean());

  ExtensionMsg_ExternalConnectionInfo info;

  // For messaging APIs, hosted apps should be considered a web page so hide
  // its extension ID.
  const Extension* extension = context()->extension();
  if (extension && !extension->is_hosted_app())
    info.source_id = extension->id();

  info.target_id = *v8::String::Utf8Value(args[0]->ToString());
  info.source_url = context()->GetURL();
  std::string channel_name = *v8::String::Utf8Value(args[1]->ToString());
  bool include_tls_channel_id =
      args.Length() > 2 ? args[2]->BooleanValue() : false;
  int port_id = -1;
  renderview->Send(
      new ExtensionHostMsg_OpenChannelToExtension(renderview->GetRoutingID(),
                                                  info,
                                                  channel_name,
                                                  include_tls_channel_id,
                                                  &port_id));
  args.GetReturnValue().Set(static_cast<int32_t>(port_id));
}

void RuntimeCustomBindings::OpenChannelToNativeApp(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Verify that the extension has permission to use native messaging.
  Feature::Availability availability =
      FeatureProvider::GetPermissionFeatures()
          ->GetFeature("nativeMessaging")
          ->IsAvailableToContext(context()->extension(),
                                 context()->context_type(),
                                 context()->GetURL());
  if (!availability.is_available())
    return;

  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = context()->GetRenderView();
  if (!renderview)
    return;

  // The Javascript code should validate/fill the arguments.
  CHECK(args.Length() >= 2 && args[0]->IsString() && args[1]->IsString());

  std::string extension_id = *v8::String::Utf8Value(args[0]->ToString());
  std::string native_app_name = *v8::String::Utf8Value(args[1]->ToString());

  int port_id = -1;
  renderview->Send(new ExtensionHostMsg_OpenChannelToNativeApp(
      renderview->GetRoutingID(), extension_id, native_app_name, &port_id));
  args.GetReturnValue().Set(static_cast<int32_t>(port_id));
}

void RuntimeCustomBindings::GetManifest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(context()->extension());

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  args.GetReturnValue().Set(converter->ToV8Value(
      context()->extension()->manifest()->value(), context()->v8_context()));
}

void RuntimeCustomBindings::GetExtensionViews(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() != 2)
    return;

  if (!args[0]->IsInt32() || !args[1]->IsString())
    return;

  // |browser_window_id| == extension_misc::kUnknownWindowId means getting
  // all views for the current extension.
  int browser_window_id = args[0]->Int32Value();

  std::string view_type_string = *v8::String::Utf8Value(args[1]->ToString());
  StringToUpperASCII(&view_type_string);
  // |view_type| == VIEW_TYPE_INVALID means getting any type of
  // views.
  ViewType view_type = VIEW_TYPE_INVALID;
  if (view_type_string == kViewTypeBackgroundPage) {
    view_type = VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
  } else if (view_type_string == kViewTypeInfobar) {
    view_type = VIEW_TYPE_EXTENSION_INFOBAR;
  } else if (view_type_string == kViewTypeTabContents) {
    view_type = VIEW_TYPE_TAB_CONTENTS;
  } else if (view_type_string == kViewTypePopup) {
    view_type = VIEW_TYPE_EXTENSION_POPUP;
  } else if (view_type_string == kViewTypeExtensionDialog) {
    view_type = VIEW_TYPE_EXTENSION_DIALOG;
  } else if (view_type_string == kViewTypeAppWindow) {
    view_type = VIEW_TYPE_APP_WINDOW;
  } else if (view_type_string == kViewTypePanel) {
    view_type = VIEW_TYPE_PANEL;
  } else if (view_type_string != kViewTypeAll) {
    return;
  }

  std::string extension_id = context()->GetExtensionID();
  if (extension_id.empty())
    return;

  std::vector<content::RenderView*> views = ExtensionHelper::GetExtensionViews(
      extension_id, browser_window_id, view_type);
  v8::Local<v8::Array> v8_views = v8::Array::New(args.GetIsolate());
  int v8_index = 0;
  for (size_t i = 0; i < views.size(); ++i) {
    v8::Local<v8::Context> context =
        views[i]->GetWebView()->mainFrame()->mainWorldScriptContext();
    if (!context.IsEmpty()) {
      v8::Local<v8::Value> window = context->Global();
      DCHECK(!window.IsEmpty());
      v8_views->Set(v8::Integer::New(args.GetIsolate(), v8_index++), window);
    }
  }

  args.GetReturnValue().Set(v8_views);
}

}  // namespace extensions
