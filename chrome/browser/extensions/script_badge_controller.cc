// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_badge_controller.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

ScriptBadgeController::ScriptBadgeController(content::WebContents* web_contents,
                                             TabHelper* tab_helper)
    : TabHelper::ScriptExecutionObserver(tab_helper),
      content::WebContentsObserver(web_contents) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile()));
}

ScriptBadgeController::~ScriptBadgeController() {}

std::vector<ExtensionAction*> ScriptBadgeController::GetCurrentActions() const {
  std::vector<ExtensionAction*> result;
  ExtensionService* service = GetExtensionService();
  if (!service)
    return result;

  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(profile());
  const ExtensionSet* extensions = service->extensions();
  for (std::set<std::string>::const_iterator
           it = extensions_in_current_actions_.begin();
       it != extensions_in_current_actions_.end(); ++it) {
    const Extension* extension = extensions->GetByID(*it);
    if (!extension)
      continue;
    ExtensionAction* script_badge =
        extension_action_manager->GetScriptBadge(*extension);
    if (script_badge)
      result.push_back(script_badge);
  }
  return result;
}

void ScriptBadgeController::GetAttentionFor(
    const std::string& extension_id) {
  ExtensionAction* script_badge = AddExtensionToCurrentActions(extension_id);
  if (!script_badge)
    return;

  // TODO(jyasskin): Modify the icon's appearance to indicate that the
  // extension is merely asking for permission to run:
  // http://crbug.com/133142
  script_badge->SetAppearance(SessionID::IdForTab(web_contents()),
                              ExtensionAction::WANTS_ATTENTION);

  NotifyChange();
}

LocationBarController::Action ScriptBadgeController::OnClicked(
    const std::string& extension_id, int mouse_button) {
  ExtensionService* service = GetExtensionService();
  if (!service)
    return ACTION_NONE;

  const Extension* extension = service->extensions()->GetByID(extension_id);
  CHECK(extension);
  ExtensionAction* script_badge =
      ExtensionActionManager::Get(profile())->GetScriptBadge(*extension);
  CHECK(script_badge);

  switch (mouse_button) {
    case 1:    // left
    case 2: {  // middle
      extensions::TabHelper::FromWebContents(web_contents())->
          active_tab_permission_granter()->GrantIfRequested(extension);

      // Even if clicking the badge doesn't immediately cause the extension to
      // run script on the page, we want to help users associate clicking with
      // the extension having permission to modify the page, so we make the icon
      // full-colored immediately.
      if (script_badge->SetAppearance(SessionID::IdForTab(web_contents()),
                                      ExtensionAction::ACTIVE))
        NotifyChange();

      // Fire the scriptBadge.onClicked event.
      GetExtensionService()->browser_event_router()->ScriptBadgeExecuted(
          profile(), *script_badge, SessionID::IdForTab(web_contents()));

      // TODO(jyasskin): The fallback order should be user-defined popup ->
      // onClicked handler -> default popup.
      return ACTION_SHOW_SCRIPT_POPUP;
    }
    case 3:  // right
      // Don't grant access on right clicks, so users can investigate
      // the extension without danger.

      return extension->ShowConfigureContextMenus() ?
          ACTION_SHOW_CONTEXT_MENU : ACTION_NONE;
  }

  return ACTION_NONE;
}

namespace {
std::string JoinExtensionIDs(const ExecutingScriptsMap& ids) {
  std::vector<std::string> as_vector;
  for (ExecutingScriptsMap::const_iterator iter = ids.begin();
       iter != ids.end(); ++iter) {
    as_vector.push_back(iter->first);
  }
  return "[" + JoinString(as_vector, ',') + "]";
}
}  // namespace

void ScriptBadgeController::OnScriptsExecuted(
    const content::WebContents* web_contents,
    const ExecutingScriptsMap& extension_ids,
    int32 on_page_id,
    const GURL& on_url) {
  int32 current_page_id = GetPageID();
  if (on_page_id != current_page_id)
    return;

  if (current_page_id < 0) {
    // Tracking down http://crbug.com/138323.
    std::string message = base::StringPrintf(
        "Expected a page ID of %d but there was no navigation entry. "
        "Extension IDs are %s.",
        on_page_id,
        JoinExtensionIDs(extension_ids).c_str());
    char buf[1024];
    base::snprintf(buf, arraysize(buf), "%s", message.c_str());
    LOG(ERROR) << message;
    return;
  }

  bool changed = false;
  for (ExecutingScriptsMap::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    changed |= MarkExtensionExecuting(it->first);
  }
  if (changed)
    NotifyChange();
}

Profile* ScriptBadgeController::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

ExtensionService* ScriptBadgeController::GetExtensionService() const {
  return ExtensionSystem::Get(profile())->extension_service();
}

int32 ScriptBadgeController::GetPageID() {
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetActiveEntry();
  return nav_entry ? nav_entry->GetPageID() : -1;
}

void ScriptBadgeController::NotifyChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(profile()),
      content::Details<content::WebContents>(web_contents()));
}

void ScriptBadgeController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  extensions_in_current_actions_.clear();
}

void ScriptBadgeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
  if (extensions_in_current_actions_.erase(extension->id()))
    NotifyChange();
}

ExtensionAction* ScriptBadgeController::AddExtensionToCurrentActions(
    const std::string& extension_id) {
  if (!extensions_in_current_actions_.insert(extension_id).second)
    return NULL;

  ExtensionService* service = GetExtensionService();
  if (!service)
    return NULL;

  const Extension* extension = service->extensions()->GetByID(extension_id);
  if (!extension)
    return NULL;

  return ExtensionActionManager::Get(profile())->GetScriptBadge(*extension);
}

bool ScriptBadgeController::MarkExtensionExecuting(
    const std::string& extension_id) {
  ExtensionAction* script_badge = AddExtensionToCurrentActions(extension_id);
  if (!script_badge)
    return false;

  script_badge->SetAppearance(SessionID::IdForTab(web_contents()),
                              ExtensionAction::ACTIVE);
  return true;
}

}  // namespace extensions
