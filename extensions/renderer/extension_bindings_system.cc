// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_bindings_system.h"

#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/externally_connectable.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

// static
bool ExtensionBindingsSystem::IsRuntimeAvailableToContext(
    ScriptContext* context) {
  for (const auto& extension :
       *RendererExtensionRegistry::Get()->GetMainThreadExtensionSet()) {
    ExternallyConnectableInfo* info = static_cast<ExternallyConnectableInfo*>(
        extension->GetManifestData(manifest_keys::kExternallyConnectable));
    if (info && info->matches.MatchesURL(context->url()))
      return true;
  }
  return false;
}

// static
const char* ExtensionBindingsSystem::kWebAvailableFeatures[] = {
    "app", "webstore", "dashboardPrivate",
};

}  // namespace extensions
