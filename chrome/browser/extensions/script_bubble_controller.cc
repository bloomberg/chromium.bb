// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_bubble_controller.h"

#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "content/public/browser/navigation_details.h"

using extensions::APIPermission;

namespace extensions {

ScriptBubbleController::ScriptBubbleController(
    content::WebContents* web_contents, TabHelper* tab_helper)
    : TabHelper::ScriptExecutionObserver(tab_helper),
      content::WebContentsObserver(web_contents) {
}

ScriptBubbleController::~ScriptBubbleController() {
}

void ScriptBubbleController::OnScriptsExecuted(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& executing_scripts,
      int32 page_id,
      const GURL& on_url) {
  DCHECK_EQ(this->web_contents(), web_contents);

  bool changed = false;
  ExtensionService* extension_service = GetExtensionService();
  for (ExecutingScriptsMap::const_iterator i = executing_scripts.begin();
       i != executing_scripts.end(); ++i) {
    // Don't display extensions that wouldn't be shown in settings because
    // those are effectively not installed from the user's point of view.
    const Extension* extension =
        extension_service->extensions()->GetByID(i->first);
    if (extension && extension->ShouldDisplayInExtensionSettings() &&
        extension->HasAPIPermission(APIPermission::kActiveTab))
      changed |= extensions_running_scripts_.insert(i->first).second;
  }

  if (changed)
    UpdateScriptBubble();
}

void ScriptBubbleController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (!details.is_navigation_to_different_page())
    return;
  extensions_running_scripts_.clear();
  UpdateScriptBubble();
}

void ScriptBubbleController::OnExtensionUnloaded(
    const std::string& extension_id) {
  if (extensions_running_scripts_.erase(extension_id) == 1)
    UpdateScriptBubble();
}

Profile* ScriptBubbleController::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

ExtensionService* ScriptBubbleController::GetExtensionService() const {
  return ExtensionSystem::Get(profile())->extension_service();
}

void ScriptBubbleController::UpdateScriptBubble() {
  tab_helper_->location_bar_controller()->NotifyChange();
}

}  // namespace extensions
