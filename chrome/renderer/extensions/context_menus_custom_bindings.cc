// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/context_menus_custom_bindings.h"

#include "grit/renderer_resources.h"
#include "v8/include/v8.h"

namespace {

v8::Handle<v8::Value> GetNextContextMenuId(const v8::Arguments& args) {
  // Note: this works because contextMenus.create() only works in the
  // extension process.  If that API is opened up to content scripts, this
  // will need to change.  See crbug.com/77023
  static int next_context_menu_id = 1;
  return v8::Integer::New(next_context_menu_id++);
}

}  // namespace

namespace extensions {

ContextMenusCustomBindings::ContextMenusCustomBindings()
    : ChromeV8Extension(NULL) {
  RouteStaticFunction("GetNextContextMenuId", &GetNextContextMenuId);
}

}  // extensions
