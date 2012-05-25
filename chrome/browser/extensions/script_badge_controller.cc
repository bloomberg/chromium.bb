// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_badge_controller.h"

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

ScriptBadgeController::ScriptBadgeController(TabContentsWrapper* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      script_executor_(tab_contents->web_contents()),
      tab_contents_(tab_contents) {
}

ScriptBadgeController::~ScriptBadgeController() {
}

scoped_ptr<std::vector<ExtensionAction*> >
ScriptBadgeController::GetCurrentActions() {
  scoped_ptr<std::vector<ExtensionAction*> > current_actions(
      new std::vector<ExtensionAction*>());

  ExtensionService* service = GetExtensionService();
  if (!service)
    return current_actions.Pass();

  const GURL& current_url = tab_contents_->web_contents()->GetURL();

  for (ExtensionSet::const_iterator it = service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
    const Extension* extension = *it;
    if (extension->HasContentScriptAtURL(current_url) ||
        extensions_executing_scripts_.count(extension->id())) {
      current_actions->push_back(GetScriptBadge(extension));
    }
  }
  return current_actions.Pass();
}

ActionBoxController::Action ScriptBadgeController::OnClicked(
    const std::string& extension_id, int mouse_button) {
  ExtensionService* service = GetExtensionService();
  if (!service)
    return ACTION_NONE;

  const Extension* extension = service->extensions()->GetByID(extension_id);
  CHECK(extension);

  switch (mouse_button) {
    case 3: // right
      return extension->ShowConfigureContextMenus() ?
          ACTION_SHOW_CONTEXT_MENU : ACTION_NONE;
  }

  return ACTION_NONE;
}

void ScriptBadgeController::ExecuteScript(
    const std::string& extension_id,
    ScriptExecutor::ScriptType script_type,
    const std::string& code,
    ScriptExecutor::FrameScope frame_scope,
    UserScript::RunLocation run_at,
    ScriptExecutor::WorldType world_type,
    const ExecuteScriptCallback& callback) {
  script_executor_.ExecuteScript(extension_id,
                                 script_type,
                                 code,
                                 frame_scope,
                                 run_at,
                                 world_type,
                                 callback);

  // This tab should now show that the extension executing a script.
  extensions_executing_scripts_.insert(extension_id);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_ACTION_BOX_UPDATED,
      content::Source<Profile>(tab_contents_->profile()),
      content::Details<TabContentsWrapper>(tab_contents_));
}

void ScriptBadgeController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  extensions_executing_scripts_.clear();
}

ExtensionAction* ScriptBadgeController::GetScriptBadge(
    const Extension* extension) {
  ScriptBadgeMap::iterator existing = script_badges_.find(extension->id());
  if (existing != script_badges_.end())
    return existing->second.get();

  linked_ptr<ExtensionAction> badge(new ExtensionAction(extension->id()));
  badge->SetTitle(ExtensionAction::kDefaultTabId, extension->name());
  badge->SetIsVisible(ExtensionAction::kDefaultTabId, true);

  // If there are existing actions, and they have default icon paths, use those.
  // Otherwise we'll need to use the default icon and set it for all tabs.
  std::string default_icon_path;

  if (extension->browser_action())
    default_icon_path = extension->browser_action()->default_icon_path();
  else if (extension->page_action())
    default_icon_path = extension->page_action()->default_icon_path();

  if (!default_icon_path.empty()) {
    badge->set_default_icon_path(default_icon_path);
  } else {
    badge->SetIcon(
        ExtensionAction::kDefaultTabId,
        *ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_EXTENSIONS_FAVICON).ToSkBitmap());
  }

  script_badges_[extension->id()] = badge;
  return badge.get();
}

ExtensionService* ScriptBadgeController::GetExtensionService() {
  return ExtensionSystem::Get(tab_contents_->profile())->extension_service();
}

}  // namespace extensions
