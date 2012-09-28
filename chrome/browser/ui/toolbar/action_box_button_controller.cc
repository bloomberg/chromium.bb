// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/action_box_button_controller.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/toolbar/action_box_menu_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {

// Extensions get command IDs that are beyond the maximal valid command ID
// (0xDFFF) so that they are not confused with actual commands that appear in
// the menu. For more details see: chrome/app/chrome_command_ids.h
const int kFirstExtensionCommandId = 0xE000;

}  // namespace

ActionBoxButtonController::ActionBoxButtonController(Browser* browser,
                                                     Delegate* delegate)
    : browser_(browser),
      delegate_(delegate),
      next_extension_command_id_(kFirstExtensionCommandId) {
  DCHECK(browser_);
  DCHECK(delegate_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(browser->profile()));
}

ActionBoxButtonController::~ActionBoxButtonController() {}

void ActionBoxButtonController::OnButtonClicked() {
  // Creating the action box menu will populate it with the bookmark star etc,
  // but no extensions.
  scoped_ptr<ActionBoxMenuModel> menu_model(
      new ActionBoxMenuModel(browser_, this));

  // Add the extensions.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service();
  const extensions::ExtensionList& extensions =
      extension_service->toolbar_model()->action_box_menu_items();
  for (extensions::ExtensionList::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    menu_model->AddExtension(**it, GetCommandIdForExtension(**it));
  }

  delegate_->ShowMenu(menu_model.Pass());
}

bool ActionBoxButtonController::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ActionBoxButtonController::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool ActionBoxButtonController::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void ActionBoxButtonController::ExecuteCommand(int command_id) {
  // It might be a command associated with an extension.
  // Note that the extension might have been uninstalled or disabled while the
  // menu was open (sync perhaps?) but that will just fall through safely.
  const extensions::Extension* extension =
      GetExtensionForCommandId(command_id);
  if (extension) {
    // TODO(kalman): do something with the result.
    extensions::ExtensionSystem::Get(browser_->profile())->
        extension_service()->toolbar_model()->ExecuteBrowserAction(
            extension, browser_, NULL);
    return;
  }

  chrome::ExecuteCommand(browser_, command_id);
}

int ActionBoxButtonController::GetCommandIdForExtension(
    const extensions::Extension& extension) {
  ExtensionIdCommandMap::iterator it =
      extension_command_ids_.find(extension.id());
  if (it != extension_command_ids_.end())
    return it->second;
  int command_id = next_extension_command_id_++;

  // Note that we deliberately don't clean up extension IDs here when
  // extensions are unloaded, so that if they're reloaded they get assigned the
  // old command ID. This situation could arise if an extension is updated
  // while the menu is open. On the other hand, we leak some memory... but
  // that's unlikely to matter.
  extension_command_ids_[extension.id()] = command_id;

  return command_id;
}

const extensions::Extension*
    ActionBoxButtonController::GetExtensionForCommandId(int command_id) {
  for (ExtensionIdCommandMap::iterator it = extension_command_ids_.begin();
       it != extension_command_ids_.end(); ++it) {
    if (it->second == command_id) {
      // Note: might be NULL anyway if the extension has been uninstalled.
      return extensions::ExtensionSystem::Get(browser_->profile())->
          extension_service()->extensions()->GetByID(it->first);
    }
  }

  return NULL;
}

void ActionBoxButtonController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const extensions::Extension* extension =
      content::Details<extensions::UnloadedExtensionInfo>(details)->extension;

  // TODO(kalman): if there's a menu open, remove it from that too.
  // We may also want to listen to EXTENSION_LOADED to do the opposite.
  extension_command_ids_.erase(extension->id());
}
