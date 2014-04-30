// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/renderer/shell_extensions_renderer_client.h"

#include "apps/shell/renderer/shell_custom_bindings.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/resource_bundle_source_map.h"
#include "extensions/renderer/module_system.h"
#include "grit/app_shell_resources.h"

namespace apps {

ShellExtensionsRendererClient::ShellExtensionsRendererClient() {}

ShellExtensionsRendererClient::~ShellExtensionsRendererClient() {}

bool ShellExtensionsRendererClient::IsIncognitoProcess() const {
  // app_shell doesn't support off-the-record contexts.
  return false;
}

int ShellExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // app_shell doesn't need to reserve world IDs for anything other than
  // extensions, so we always return 1. Note that 0 is reserved for the global
  // world.
  return 1;
}

void ShellExtensionsRendererClient::RegisterNativeHandlers(
    extensions::ModuleSystem* module_system,
    extensions::ScriptContext* context) {
  module_system->RegisterNativeHandler(
      "shell_natives",
      scoped_ptr<extensions::NativeHandler>(new ShellCustomBindings(context)));
}

void ShellExtensionsRendererClient::PopulateSourceMap(
    ResourceBundleSourceMap* source_map) {
  source_map->RegisterSource("shell", IDR_SHELL_CUSTOM_BINDINGS_JS);
}

}  // namespace apps
