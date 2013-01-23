// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/runtime_custom_bindings.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/v8_value_converter.h"

using content::V8ValueConverter;

namespace extensions {

RuntimeCustomBindings::RuntimeCustomBindings(ChromeV8Context* context)
    : ChromeV8Extension(NULL), context_(context) {
  RouteFunction("GetManifest",
      base::Bind(&RuntimeCustomBindings::GetManifest, base::Unretained(this)));
  RouteStaticFunction("OpenChannelToExtension", &OpenChannelToExtension);
  RouteStaticFunction("OpenChannelToNativeApp", &OpenChannelToNativeApp);
}

RuntimeCustomBindings::~RuntimeCustomBindings() {}

// static
v8::Handle<v8::Value> RuntimeCustomBindings::OpenChannelToExtension(
    const v8::Arguments& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = GetCurrentRenderView();
  if (!renderview)
    return v8::Undefined();

  // The Javascript code should validate/fill the arguments.
  CHECK(args.Length() >= 3 &&
        args[0]->IsString() &&
        args[1]->IsString() &&
        args[2]->IsString());

  std::string source_id = *v8::String::Utf8Value(args[0]->ToString());
  std::string target_id = *v8::String::Utf8Value(args[1]->ToString());
  std::string channel_name = *v8::String::Utf8Value(args[2]->ToString());
  int port_id = -1;
  renderview->Send(new ExtensionHostMsg_OpenChannelToExtension(
      renderview->GetRoutingID(),
      source_id,
      target_id,
      channel_name,
      &port_id));
  return v8::Integer::New(port_id);
}

// static
v8::Handle<v8::Value> RuntimeCustomBindings::OpenChannelToNativeApp(
    const v8::Arguments& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = GetCurrentRenderView();
  if (!renderview)
    return v8::Undefined();

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
  return v8::Integer::New(port_id);
}

v8::Handle<v8::Value> RuntimeCustomBindings::GetManifest(
    const v8::Arguments& args) {
  CHECK(context_->extension());

  scoped_ptr<V8ValueConverter> converter(V8ValueConverter::create());
  return converter->ToV8Value(context_->extension()->manifest()->value(),
                              context_->v8_context());
}

}  // extensions
