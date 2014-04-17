// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/tabs_custom_bindings.h"

#include <string>

#include "base/bind.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8.h"

namespace extensions {

TabsCustomBindings::TabsCustomBindings(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("OpenChannelToTab",
      base::Bind(&TabsCustomBindings::OpenChannelToTab,
                 base::Unretained(this)));
}

void TabsCustomBindings::OpenChannelToTab(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderView* renderview = context()->GetRenderView();
  if (!renderview)
    return;

  if (args.Length() >= 3 && args[0]->IsInt32() && args[1]->IsString() &&
      args[2]->IsString()) {
    int tab_id = args[0]->Int32Value();
    std::string extension_id = *v8::String::Utf8Value(args[1]->ToString());
    std::string channel_name = *v8::String::Utf8Value(args[2]->ToString());
    int port_id = -1;
    renderview->Send(new ExtensionHostMsg_OpenChannelToTab(
      renderview->GetRoutingID(), tab_id, extension_id, channel_name,
        &port_id));
    args.GetReturnValue().Set(static_cast<int32_t>(port_id));
    return;
  }
}

}  // namespace extensions
