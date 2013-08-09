// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/runtime_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/renderer/extensions/api_activity_logger.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/dispatcher.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/common/features/feature_provider.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"

using content::V8ValueConverter;

namespace extensions {

RuntimeCustomBindings::RuntimeCustomBindings(Dispatcher* dispatcher,
                                             ChromeV8Context* context)
    : ChromeV8Extension(dispatcher, context) {
  RouteFunction("GetManifest",
                base::Bind(&RuntimeCustomBindings::GetManifest,
                           base::Unretained(this)));
  RouteFunction("OpenChannelToExtension",
                base::Bind(&RuntimeCustomBindings::OpenChannelToExtension,
                           base::Unretained(this)));
  RouteFunction("OpenChannelToNativeApp",
                base::Bind(&RuntimeCustomBindings::OpenChannelToNativeApp,
                           base::Unretained(this)));
}

RuntimeCustomBindings::~RuntimeCustomBindings() {}

void RuntimeCustomBindings::OpenChannelToExtension(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = GetRenderView();
  if (!renderview)
    return;

  // The Javascript code should validate/fill the arguments.
  CHECK_EQ(2, args.Length());
  CHECK(args[0]->IsString() && args[1]->IsString());

  ExtensionMsg_ExternalConnectionInfo info;
  info.source_id = context()->extension() ? context()->extension()->id() : "";
  info.target_id = *v8::String::Utf8Value(args[0]->ToString());
  info.source_url = renderview->GetWebView()->mainFrame()->document().url();
  std::string channel_name = *v8::String::Utf8Value(args[1]->ToString());
  int port_id = -1;
  renderview->Send(new ExtensionHostMsg_OpenChannelToExtension(
      renderview->GetRoutingID(), info, channel_name, &port_id));
  args.GetReturnValue().Set(static_cast<int32_t>(port_id));
}

void RuntimeCustomBindings::OpenChannelToNativeApp(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Verify that the extension has permission to use native messaging.
  Feature::Availability availability =
      FeatureProvider::GetByName("permission")->
          GetFeature("nativeMessaging")->IsAvailableToContext(
              GetExtensionForRenderView(),
              context()->context_type(),
              context()->GetURL());
  if (!availability.is_available()) {
    APIActivityLogger::LogBlockedCall(context()->extension()->id(),
                                      "nativeMessaging",
                                      availability.result());
    return;
  }

  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = GetRenderView();
  if (!renderview)
    return;

  // The Javascript code should validate/fill the arguments.
  CHECK(args.Length() >= 2 &&
        args[0]->IsString() &&
        args[1]->IsString());

  std::string extension_id = *v8::String::Utf8Value(args[0]->ToString());
  std::string native_app_name = *v8::String::Utf8Value(args[1]->ToString());

  int port_id = -1;
  renderview->Send(new ExtensionHostMsg_OpenChannelToNativeApp(
      renderview->GetRoutingID(),
      extension_id,
      native_app_name,
      &port_id));
  args.GetReturnValue().Set(static_cast<int32_t>(port_id));
}

void RuntimeCustomBindings::GetManifest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(context()->extension());

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  args.GetReturnValue().Set(
      converter->ToV8Value(context()->extension()->manifest()->value(),
                           context()->v8_context()));
}

}  // namespace extensions
