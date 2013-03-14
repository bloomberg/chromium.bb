// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/context_menus_custom_bindings.h"

#include "base/bind.h"
#include "chrome/common/extensions/extension_messages.h"
#include "content/public/renderer/render_thread.h"
#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace {

v8::Handle<v8::Value> GetNextContextMenuId(const v8::Arguments& args) {
  int context_menu_id = -1;
  content::RenderThread::Get()->Send(
      new ExtensionHostMsg_GenerateUniqueID(&context_menu_id));
  return v8::Integer::New(context_menu_id);
}

}  // namespace

namespace extensions {

ContextMenusCustomBindings::ContextMenusCustomBindings(
    Dispatcher* dispatcher, v8::Handle<v8::Context> v8_context)
    : ChromeV8Extension(dispatcher, v8_context) {
  RouteFunction("GetNextContextMenuId", base::Bind(&GetNextContextMenuId));
}

}  // extensions
