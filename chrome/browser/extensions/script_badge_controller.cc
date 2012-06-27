// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_badge_controller.h"

#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

ScriptBadgeController::ScriptBadgeController(TabContents* tab_contents)
    : content::WebContentsObserver(tab_contents->web_contents()),
      script_executor_(tab_contents->web_contents()),
      tab_contents_(tab_contents) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(tab_contents->profile()));
}

ScriptBadgeController::~ScriptBadgeController() {}

std::vector<ExtensionAction*> ScriptBadgeController::GetCurrentActions() const {
  return current_actions_;
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
      return ACTION_SHOW_SCRIPT_POPUP;
    case 2:  // middle
      // TODO(yoz): Show the popup if it's available or a default if not.

      // Fire the scriptBadge.onClicked event.
      GetExtensionService()->browser_event_router()->ScriptBadgeExecuted(
          tab_contents_->profile(),
          *script_badge,
          tab_contents_->extension_tab_helper()->tab_id());
      return ACTION_NONE;
    case 3:  // right
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
  ExecuteScriptCallback this_callback = base::Bind(
      &ScriptBadgeController::OnExecuteScriptFinished,
      this,
      extension_id,
      callback);

  script_executor_.ExecuteScript(extension_id,
                                 script_type,
                                 code,
                                 frame_scope,
                                 run_at,
                                 world_type,
                                 this_callback);
}

void ScriptBadgeController::OnExecuteScriptFinished(
    const std::string& extension_id,
    const ExecuteScriptCallback& callback,
    bool success,
    int32 page_id,
    const std::string& error) {
  if (success && page_id == GetPageID()) {
    if (InsertExtension(extension_id))
      NotifyChange();
  }

  callback.Run(success, page_id, error);
}

ExtensionService* ScriptBadgeController::GetExtensionService() {
  return ExtensionSystem::Get(tab_contents_->profile())->extension_service();
}

int32 ScriptBadgeController::GetPageID() {
  return tab_contents_->web_contents()->GetController().GetActiveEntry()->
      GetPageID();
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
  extensions_executing_scripts_.clear();
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

void ScriptBadgeController::OnContentScriptsExecuting(
    const std::set<std::string>& extension_ids, int32 page_id) {
  if (page_id != GetPageID())
    return;

  bool changed = false;
  for (std::set<std::string>::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    changed |= InsertExtension(*it);
  }
  if (changed)
    NotifyChange();
}

bool ScriptBadgeController::InsertExtension(const std::string& extension_id) {
  if (!extensions_executing_scripts_.insert(extension_id).second)
    return false;

  ExtensionService* service = GetExtensionService();
  if (!service)
    return  false;

  const Extension* extension = service->extensions()->GetByID(extension_id);
  if (!extension)
    return false;

  ExtensionAction* script_badge = extension->script_badge();
  current_actions_.push_back(script_badge);
  script_badge->RunIconAnimation(
      tab_contents_->extension_tab_helper()->tab_id());

  return true;
}

bool ScriptBadgeController::EraseExtension(const Extension* extension) {
  if (extensions_executing_scripts_.erase(extension->id()) == 0)
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
