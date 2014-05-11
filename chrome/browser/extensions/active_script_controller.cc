// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_script_controller.h"

#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

ActiveScriptController::ActiveScriptController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      enabled_(FeatureSwitch::scripts_require_action()->IsEnabled()) {
}

ActiveScriptController::~ActiveScriptController() {
  LogUMA();
}

void ActiveScriptController::NotifyScriptExecuting(
    const std::string& extension_id, int page_id) {
  if (extensions_executing_scripts_.count(extension_id) ||
      web_contents()->GetController().GetVisibleEntry()->GetPageID() !=
          page_id) {
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext())
          ->enabled_extensions().GetByID(extension_id);
  if (extension &&
      PermissionsData::RequiresActionForScriptExecution(extension)) {
    extensions_executing_scripts_.insert(extension_id);
    LocationBarController::NotifyChange(web_contents());
  }
}

void ActiveScriptController::OnAdInjectionDetected(
    const std::vector<std::string> ad_injectors) {
  size_t num_preventable_ad_injectors =
      base::STLSetIntersection<std::set<std::string> >(
          ad_injectors, extensions_executing_scripts_).size();

  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ActiveScriptController.PreventableAdInjectors",
      num_preventable_ad_injectors);
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ActiveScriptController.PreventableAdInjectors",
      ad_injectors.size() - num_preventable_ad_injectors);
}

ExtensionAction* ActiveScriptController::GetActionForExtension(
    const Extension* extension) {
  if (!enabled_ || extensions_executing_scripts_.count(extension->id()) == 0)
    return NULL;  // No action for this extension.

  ActiveScriptMap::iterator existing =
      active_script_actions_.find(extension->id());
  if (existing != active_script_actions_.end())
    return existing->second.get();

  linked_ptr<ExtensionAction> action(new ExtensionAction(
      extension->id(), ActionInfo::TYPE_PAGE, ActionInfo()));
  action->SetTitle(ExtensionAction::kDefaultTabId, extension->name());
  action->SetIsVisible(ExtensionAction::kDefaultTabId, true);

  const ActionInfo* action_info = ActionInfo::GetPageActionInfo(extension);
  if (!action_info)
    action_info = ActionInfo::GetBrowserActionInfo(extension);

  if (action_info && !action_info->default_icon.empty()) {
    action->set_default_icon(
        make_scoped_ptr(new ExtensionIconSet(action_info->default_icon)));
  }

  active_script_actions_[extension->id()] = action;
  return action.get();
}

LocationBarController::Action ActiveScriptController::OnClicked(
    const Extension* extension) {
  DCHECK(extensions_executing_scripts_.count(extension->id()) > 0);
  return LocationBarController::ACTION_NONE;
}

void ActiveScriptController::OnNavigated() {
  LogUMA();
  extensions_executing_scripts_.clear();
}

bool ActiveScriptController::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ActiveScriptController, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_NotifyExtensionScriptExecution,
                        OnNotifyExtensionScriptExecution)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ActiveScriptController::OnNotifyExtensionScriptExecution(
    const std::string& extension_id,
    int page_id) {
  if (!Extension::IdIsValid(extension_id)) {
    NOTREACHED() << "'" << extension_id << "' is not a valid id.";
    return;
  }
  NotifyScriptExecuting(extension_id, page_id);
}

void ActiveScriptController::LogUMA() const {
  UMA_HISTOGRAM_COUNTS_100(
      "Extensions.ActiveScriptController.ShownActiveScriptsOnPage",
      extensions_executing_scripts_.size());
}

}  // namespace extensions
