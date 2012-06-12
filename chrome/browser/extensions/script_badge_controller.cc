// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/script_badge_controller.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
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
      tab_contents_(tab_contents) {}

ScriptBadgeController::~ScriptBadgeController() {}

scoped_ptr<std::vector<ExtensionAction*> >
ScriptBadgeController::GetCurrentActions() {
  scoped_ptr<std::vector<ExtensionAction*> > current_actions(
      new std::vector<ExtensionAction*>());

  ExtensionService* service = GetExtensionService();
  if (!service)
    return current_actions.Pass();

  for (ExtensionSet::const_iterator it = service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
    const Extension* extension = *it;
    if (extensions_executing_scripts_.count(extension->id()))
      current_actions->push_back(extension->GetScriptBadge());
  }
  return current_actions.Pass();
}

LocationBarController::Action ScriptBadgeController::OnClicked(
    const std::string& extension_id, int mouse_button) {
  ExtensionService* service = GetExtensionService();
  if (!service)
    return ACTION_NONE;

  const Extension* extension = service->extensions()->GetByID(extension_id);
  CHECK(extension);

  switch (mouse_button) {
    case 1:  // left
    case 2:  // middle
      // TODO(kalman): decide what to do here.
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
    extensions_executing_scripts_.insert(extension_id);
    Notify();
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

void ScriptBadgeController::Notify() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOCATION_BAR_UPDATED,
      content::Source<Profile>(tab_contents_->profile()),
      content::Details<TabContents>(tab_contents_));
}

void ScriptBadgeController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  extensions_executing_scripts_.clear();
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
  if (page_id == GetPageID()) {
    size_t original_size = extensions_executing_scripts_.size();
    extensions_executing_scripts_.insert(extension_ids.begin(),
                                         extension_ids.end());
    if (extensions_executing_scripts_.size() > original_size)
      Notify();
  }
}

}  // namespace extensions
