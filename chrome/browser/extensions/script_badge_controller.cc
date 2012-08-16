// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_badge_controller.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
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

ScriptBadgeController::ScriptBadgeController(TabContents* tab_contents,
                                             ScriptExecutor* script_executor)
    : ScriptExecutor::Observer(script_executor),
      content::WebContentsObserver(tab_contents->web_contents()),
      tab_contents_(tab_contents) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(tab_contents->profile()));
}

ScriptBadgeController::~ScriptBadgeController() {}

std::vector<ExtensionAction*> ScriptBadgeController::GetCurrentActions() const {
  return current_actions_;
}

void ScriptBadgeController::GetAttentionFor(
    const std::string& extension_id) {
  ExtensionAction* script_badge = AddExtensionToCurrentActions(extension_id);
  if (!script_badge)
    return;

  // TODO(jyasskin): Modify the icon's appearance to indicate that the
  // extension is merely asking for permission to run:
  // http://crbug.com/133142
  script_badge->SetAppearance(SessionID::IdForTab(tab_contents_),
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
  ExtensionAction* script_badge = extension->script_badge();
  CHECK(script_badge);

  switch (mouse_button) {
    case 1:  // left
    case 2:  // middle
      tab_contents_->extension_tab_helper()->active_tab_permission_manager()->
          GrantIfRequested(extension);

      // Even if clicking the badge doesn't immediately cause the extension to
      // run script on the page, we want to help users associate clicking with
      // the extension having permission to modify the page, so we make the icon
      // full-colored immediately.
      if (script_badge->SetAppearance(SessionID::IdForTab(tab_contents_),
                                      ExtensionAction::ACTIVE))
        NotifyChange();

      // Fire the scriptBadge.onClicked event.
      GetExtensionService()->browser_event_router()->ScriptBadgeExecuted(
          tab_contents_->profile(),
          *script_badge,
          SessionID::IdForTab(tab_contents_));

      // TODO(jyasskin): The fallback order should be user-defined popup ->
      // onClicked handler -> default popup.
      return ACTION_SHOW_SCRIPT_POPUP;
    case 3:  // right
      // Don't grant access on right clicks, so users can investigate
      // the extension without danger.

      return extension->ShowConfigureContextMenus() ?
          ACTION_SHOW_CONTEXT_MENU : ACTION_NONE;
  }

  return ACTION_NONE;
}

void ScriptBadgeController::OnExecuteScriptFinished(
    const std::string& extension_id,
    const std::string& error,
    int32 on_page_id,
    const GURL& on_url,
    const base::ListValue& script_results) {
  if (!error.empty())
    return;

  int32 current_page_id = GetPageID();

  if (on_page_id == current_page_id) {
    if (MarkExtensionExecuting(extension_id))
      NotifyChange();
  } else if (current_page_id < 0) {
    // Tracking down http://crbug.com/138323.
    std::string message = base::StringPrintf(
        "Expected a page ID of %d but there was no navigation entry. "
        "Extension ID is %s.",
        on_page_id,
        extension_id.c_str());
    char buf[1024];
    base::snprintf(buf, arraysize(buf), "%s", message.c_str());
    LOG(ERROR) << message;
    return;
  }
}

ExtensionService* ScriptBadgeController::GetExtensionService() {
  return extensions::ExtensionSystem::Get(
      tab_contents_->profile())->extension_service();
}

int32 ScriptBadgeController::GetPageID() {
  content::NavigationEntry* nav_entry =
      tab_contents_->web_contents()->GetController().GetActiveEntry();
  return nav_entry ? nav_entry->GetPageID() : -1;
}

void ScriptBadgeController::NotifyChange() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(tab_contents_->profile()),
      content::Details<TabContents>(tab_contents_));
}

void ScriptBadgeController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;
  extensions_in_current_actions_.clear();
  current_actions_.clear();
}

void ScriptBadgeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
  if (EraseExtension(extension))
    NotifyChange();
}

bool ScriptBadgeController::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ScriptBadgeController, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ContentScriptsExecuting,
                        OnContentScriptsExecuting)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

namespace {
std::string JoinExtensionIDs(const std::set<std::string>& ids) {
  std::vector<std::string> as_vector(ids.begin(), ids.end());
  return "[" + JoinString(as_vector, ',') + "]";
}
}  // namespace

void ScriptBadgeController::OnContentScriptsExecuting(
    const std::set<std::string>& extension_ids,
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
  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    changed |= MarkExtensionExecuting(*it);
  }
  if (changed)
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

  ExtensionAction* script_badge = extension->script_badge();
  current_actions_.push_back(script_badge);
  return script_badge;
}

bool ScriptBadgeController::MarkExtensionExecuting(
    const std::string& extension_id) {
  ExtensionAction* script_badge = AddExtensionToCurrentActions(extension_id);
  if (!script_badge)
    return false;

  script_badge->SetAppearance(SessionID::IdForTab(tab_contents_),
                              ExtensionAction::ACTIVE);
  return true;
}

bool ScriptBadgeController::EraseExtension(const Extension* extension) {
  if (extensions_in_current_actions_.erase(extension->id()) == 0)
    return false;

  size_t size_before = current_actions_.size();

  for (std::vector<ExtensionAction*>::iterator it = current_actions_.begin();
       it != current_actions_.end(); ++it) {
    // Safe to -> the extension action because we still have a handle to the
    // owner Extension.
    //
    // Also note that this means that when extensions are uninstalled their
    // script badges will disappear, even though they're still acting on the
    // page (since they would have already acted).
    if ((*it)->extension_id() == extension->id()) {
      current_actions_.erase(it);
      break;
    }
  }

  CHECK_EQ(size_before, current_actions_.size() + 1);
  return true;
}

}  // namespace extensions
