// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/renderer/shell_dispatcher_delegate.h"

#include "apps/shell/renderer/shell_custom_bindings.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/script_context.h"
#include "grit/app_shell_resources.h"

namespace apps {

ShellDispatcherDelegate::ShellDispatcherDelegate() {
}

ShellDispatcherDelegate::~ShellDispatcherDelegate() {
}

void ShellDispatcherDelegate::RegisterNativeHandlers(
    extensions::Dispatcher* dispatcher,
    extensions::ModuleSystem* module_system,
    extensions::ScriptContext* context) {
  module_system->RegisterNativeHandler(
      "shell_natives",
      scoped_ptr<extensions::NativeHandler>(new ShellCustomBindings(context)));
}

void ShellDispatcherDelegate::PopulateSourceMap(
    extensions::ResourceBundleSourceMap* source_map) {
  source_map->RegisterSource("shell", IDR_SHELL_CUSTOM_BINDINGS_JS);
}

}  // namespace apps
