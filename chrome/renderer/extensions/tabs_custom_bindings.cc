// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/tabs_custom_bindings.h"

#include <string>

#include "chrome/common/extensions/extension_messages.h"
#include "content/public/renderer/render_view.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace extensions {

TabsCustomBindings::TabsCustomBindings(Dispatcher* dispatcher,
                                       v8::Handle<v8::Context> context)
    : ChromeV8Extension(dispatcher, context) {
  RouteStaticFunction("OpenChannelToTab", &OpenChannelToTab);
}

// static
v8::Handle<v8::Value> TabsCustomBindings::OpenChannelToTab(
    const v8::Arguments& args) {
  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  TabsCustomBindings* self = GetFromArguments<TabsCustomBindings>(args);
  content::RenderView* renderview = self->GetRenderView();
  if (!renderview)
    return v8::Undefined();

  if (args.Length() >= 3 && args[0]->IsInt32() && args[1]->IsString() &&
      args[2]->IsString()) {
    int tab_id = args[0]->Int32Value();
    std::string extension_id = *v8::String::Utf8Value(args[1]->ToString());
    std::string channel_name = *v8::String::Utf8Value(args[2]->ToString());
    int port_id = -1;
    renderview->Send(new ExtensionHostMsg_OpenChannelToTab(
      renderview->GetRoutingID(), tab_id, extension_id, channel_name,
        &port_id));
    return v8::Integer::New(port_id);
  }
  return v8::Undefined();
}

}  // extensions
