// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/extension_action/script_badge_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/extensions/api/extension_action/script_badge_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

static base::LazyInstance<ProfileKeyedAPIFactory<ScriptBadgeAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

ScriptBadgeAPI::ScriptBadgeAPI(Profile* profile) {
  ManifestHandler::Register(extension_manifest_keys::kScriptBadge,
                            new ScriptBadgeHandler);
  ExtensionFunctionRegistry* registry =
      ExtensionFunctionRegistry::GetInstance();
  registry->RegisterFunction<ScriptBadgeGetAttentionFunction>();
  registry->RegisterFunction<ScriptBadgeGetPopupFunction>();
  registry->RegisterFunction<ScriptBadgeSetPopupFunction>();
}

ScriptBadgeAPI::~ScriptBadgeAPI() {
}

// static
ProfileKeyedAPIFactory<ScriptBadgeAPI>* ScriptBadgeAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

ScriptBadgeGetAttentionFunction::~ScriptBadgeGetAttentionFunction() {
}

bool ScriptBadgeGetAttentionFunction::RunExtensionAction() {
  tab_helper().location_bar_controller()->GetAttentionFor(extension_id());
  return true;
}

}  // namespace extensions
