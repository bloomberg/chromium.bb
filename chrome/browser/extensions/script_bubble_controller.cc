// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_bubble_controller.h"

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

namespace {

const SkColor kBadgeBackgroundColor = 0xEEEEDD00;

}  // namespace

ScriptBubbleController::ScriptBubbleController(
    content::WebContents* web_contents, TabHelper* tab_helper)
    : TabHelper::ContentScriptObserver(tab_helper),
      content::WebContentsObserver(web_contents) {
}

ScriptBubbleController::~ScriptBubbleController() {
}

GURL ScriptBubbleController::GetPopupUrl(
    const Extension* script_bubble,
    const std::set<std::string>& extension_ids) {
  return script_bubble->GetResourceURL(
      std::string("popup.html#") +
      JoinString(std::vector<std::string>(extension_ids.begin(),
                                          extension_ids.end()), ','));
}

void ScriptBubbleController::OnContentScriptsExecuting(
      const content::WebContents* web_contents,
      const ExecutingScriptsMap& executing_scripts,
      int32 page_id,
      const GURL& on_url) {
  DCHECK_EQ(this->web_contents(), web_contents);

  ExtensionService* extension_service = GetExtensionService();
  for (ExecutingScriptsMap::const_iterator i = executing_scripts.begin();
       i != executing_scripts.end(); ++i) {
    // Don't display extensions that wouldn't be shown in settings because
    // those are effectively not installed from the user's point of view.
    const Extension* extension =
        extension_service->extensions()->GetByID(i->first);
    if (extension->ShouldDisplayInExtensionSettings())
      executing_extension_ids_.insert(i->first);
  }

  UpdateScriptBubble();
}

void ScriptBubbleController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  executing_extension_ids_.clear();
  UpdateScriptBubble();
}

Profile* ScriptBubbleController::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

ExtensionService* ScriptBubbleController::GetExtensionService() const {
  return ExtensionSystem::Get(profile())->extension_service();
}

void ScriptBubbleController::UpdateScriptBubble() {
  ComponentLoader* loader = GetExtensionService()->component_loader();
  const Extension* script_bubble = loader->GetScriptBubble();
  if (!script_bubble)
    return;

  ExtensionAction* script_bubble_action =
      ExtensionActionManager::Get(profile())->GetPageAction(*script_bubble);

  ExtensionAction::Appearance appearance = ExtensionAction::INVISIBLE;
  std::string badge_text;
  GURL popup_url;

  if (!executing_extension_ids_.empty()) {
    appearance = ExtensionAction::ACTIVE;
    badge_text = base::UintToString(executing_extension_ids_.size());
    popup_url = GetPopupUrl(script_bubble, executing_extension_ids_);
  }

  int tab_id = ExtensionTabUtil::GetTabId(web_contents());

  script_bubble_action->SetAppearance(tab_id, appearance);
  script_bubble_action->SetBadgeText(tab_id, badge_text);
  script_bubble_action->SetPopupUrl(tab_id, popup_url);
  script_bubble_action->SetBadgeBackgroundColor(tab_id, kBadgeBackgroundColor);

  tab_helper_->location_bar_controller()->NotifyChange();
}

}  // namespace extensions
