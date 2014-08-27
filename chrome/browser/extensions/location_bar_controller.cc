// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/location_bar_controller.h"

#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

LocationBarController::LocationBarController(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      active_script_controller_(new ActiveScriptController(web_contents_)),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

LocationBarController::~LocationBarController() {
}

std::vector<ExtensionAction*> LocationBarController::GetCurrentActions() {
  const ExtensionSet& extensions =
      ExtensionRegistry::Get(browser_context_)->enabled_extensions();
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(browser_context_);
  std::vector<ExtensionAction*> current_actions;
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    // Right now, we can consolidate these actions because we only want to show
    // one action per extension. If clicking on an active script action ever
    // has a response, then we will need to split the actions.
    ExtensionAction* action = action_manager->GetPageAction(**iter);
    if (!action)
      action = active_script_controller_->GetActionForExtension(iter->get());
    if (action)
      current_actions.push_back(action);
  }

  return current_actions;
}

void LocationBarController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  bool should_update = false;
  if (ExtensionActionManager::Get(browser_context_)->GetPageAction(*extension))
    should_update = true;

  if (active_script_controller_->GetActionForExtension(extension)) {
    active_script_controller_->OnExtensionUnloaded(extension);
    should_update = true;
  }

  if (should_update) {
    ExtensionActionAPI::Get(browser_context)->
        NotifyPageActionsChanged(web_contents_);
  }
}

}  // namespace extensions
