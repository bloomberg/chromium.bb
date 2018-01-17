// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include <memory>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/blocked_action_bubble_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

namespace {

// The blocked actions that require a page refresh to run.
const int kRefreshRequiredActionsMask =
    BLOCKED_ACTION_WEB_REQUEST | BLOCKED_ACTION_SCRIPT_AT_START;
}

ExtensionActionRunner::PendingScript::PendingScript(
    UserScript::RunLocation run_location,
    const base::Closure& permit_script)
    : run_location(run_location), permit_script(permit_script) {}

ExtensionActionRunner::PendingScript::PendingScript(
    const PendingScript& other) = default;

ExtensionActionRunner::PendingScript::~PendingScript() {}

ExtensionActionRunner::ExtensionActionRunner(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      num_page_requests_(0),
      browser_context_(web_contents->GetBrowserContext()),
      was_used_on_page_(false),
      ignore_active_tab_granted_(false),
      extension_registry_observer_(this),
      weak_factory_(this) {
  CHECK(web_contents);
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

ExtensionActionRunner::~ExtensionActionRunner() {
  LogUMA();
}

// static
ExtensionActionRunner* ExtensionActionRunner::GetForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  return tab_helper ? tab_helper->extension_action_runner() : NULL;
}

ExtensionAction::ShowAction ExtensionActionRunner::RunAction(
    const Extension* extension,
    bool grant_tab_permissions) {
  if (grant_tab_permissions) {
    int blocked = GetBlockedActions(extension);
    if ((blocked & kRefreshRequiredActionsMask) != 0) {
      ShowBlockedActionBubble(extension);
      return ExtensionAction::ACTION_NONE;
    }
    TabHelper::FromWebContents(web_contents())
        ->active_tab_permission_granter()
        ->GrantIfRequested(extension);
    // If the extension had blocked actions, granting active tab will have
    // run the extension. Don't execute further since clicking should run
    // blocked actions *or* the normal extension action, not both.
    if (blocked != BLOCKED_ACTION_NONE)
      return ExtensionAction::ACTION_NONE;
  }

  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)
          ->GetExtensionAction(*extension);

  // Anything that gets here should have a page or browser action.
  DCHECK(extension_action);
  int tab_id = SessionTabHelper::IdForTab(web_contents());
  if (!extension_action->GetIsVisible(tab_id))
    return ExtensionAction::ACTION_NONE;

  if (extension_action->HasPopup(tab_id))
    return ExtensionAction::ACTION_SHOW_POPUP;

  ExtensionActionAPI::Get(browser_context_)
      ->DispatchExtensionActionClicked(*extension_action, web_contents(),
                                       extension);
  return ExtensionAction::ACTION_NONE;
}

void ExtensionActionRunner::RunBlockedActions(const Extension* extension) {
  DCHECK(base::ContainsKey(pending_scripts_, extension->id()) ||
         web_request_blocked_.count(extension->id()) != 0);

  // Clicking to run the extension counts as granting it permission to run on
  // the given tab.
  // The extension may already have active tab at this point, but granting
  // it twice is essentially a no-op.
  TabHelper::FromWebContents(web_contents())
      ->active_tab_permission_granter()
      ->GrantIfRequested(extension);

  RunPendingScriptsForExtension(extension);
  web_request_blocked_.erase(extension->id());

  // The extension ran, so we need to tell the ExtensionActionAPI that we no
  // longer want to act.
  NotifyChange(extension);
}

void ExtensionActionRunner::OnActiveTabPermissionGranted(
    const Extension* extension) {
  if (!ignore_active_tab_granted_ && WantsToRun(extension))
    RunBlockedActions(extension);
}

void ExtensionActionRunner::OnWebRequestBlocked(const Extension* extension) {
  web_request_blocked_.insert(extension->id());
}

int ExtensionActionRunner::GetBlockedActions(const Extension* extension) {
  int blocked_actions = BLOCKED_ACTION_NONE;
  if (web_request_blocked_.count(extension->id()) != 0)
    blocked_actions |= BLOCKED_ACTION_WEB_REQUEST;
  auto iter = pending_scripts_.find(extension->id());
  if (iter != pending_scripts_.end()) {
    for (const PendingScript& script : iter->second) {
      switch (script.run_location) {
        case UserScript::DOCUMENT_START:
          blocked_actions |= BLOCKED_ACTION_SCRIPT_AT_START;
          break;
        case UserScript::DOCUMENT_END:
        case UserScript::DOCUMENT_IDLE:
        case UserScript::BROWSER_DRIVEN:
          blocked_actions |= BLOCKED_ACTION_SCRIPT_OTHER;
          break;
        case UserScript::UNDEFINED:
        case UserScript::RUN_DEFERRED:
        case UserScript::RUN_LOCATION_LAST:
          NOTREACHED();
      }
    }
  }

  return blocked_actions;
}

bool ExtensionActionRunner::WantsToRun(const Extension* extension) {
  return GetBlockedActions(extension) != BLOCKED_ACTION_NONE;
}

void ExtensionActionRunner::RunForTesting(const Extension* extension) {
  if (WantsToRun(extension)) {
    TabHelper::FromWebContents(web_contents())
          ->active_tab_permission_granter()
          ->GrantIfRequested(extension);
  }
}

PermissionsData::AccessType
ExtensionActionRunner::RequiresUserConsentForScriptInjection(
    const Extension* extension,
    UserScript::InjectionType type) {
  CHECK(extension);

  // Allow the extension if it's been explicitly granted permission.
  if (permitted_extensions_.count(extension->id()) > 0)
    return PermissionsData::ACCESS_ALLOWED;

  GURL url = web_contents()->GetVisibleURL();
  int tab_id = SessionTabHelper::IdForTab(web_contents());
  switch (type) {
    case UserScript::CONTENT_SCRIPT:
      return extension->permissions_data()->GetContentScriptAccess(
          extension, url, tab_id, nullptr);
    case UserScript::PROGRAMMATIC_SCRIPT:
      return extension->permissions_data()->GetPageAccess(extension, url,
                                                          tab_id, nullptr);
  }

  NOTREACHED();
  return PermissionsData::ACCESS_DENIED;
}

void ExtensionActionRunner::RequestScriptInjection(
    const Extension* extension,
    UserScript::RunLocation run_location,
    const base::Closure& callback) {
  CHECK(extension);
  PendingScriptList& list = pending_scripts_[extension->id()];
  list.push_back(PendingScript(run_location, callback));

  // If this was the first entry, we need to notify that a new extension wants
  // to run.
  if (list.size() == 1u)
    NotifyChange(extension);

  was_used_on_page_ = true;
}

void ExtensionActionRunner::RunPendingScriptsForExtension(
    const Extension* extension) {
  DCHECK(extension);

  content::NavigationEntry* visible_entry =
      web_contents()->GetController().GetVisibleEntry();
  // Refuse to run if there's no visible entry, because we have no idea of
  // determining if it's the proper page. This should rarely, if ever, happen.
  if (!visible_entry)
    return;

  // We add this to the list of permitted extensions and erase pending entries
  // *before* running them to guard against the crazy case where running the
  // callbacks adds more entries.
  permitted_extensions_.insert(extension->id());

  PendingScriptMap::iterator iter = pending_scripts_.find(extension->id());
  if (iter == pending_scripts_.end())
    return;

  PendingScriptList scripts;
  iter->second.swap(scripts);
  pending_scripts_.erase(extension->id());

  // Run all pending injections for the given extension.
  for (PendingScript& pending_script : scripts)
    pending_script.permit_script.Run();
}

void ExtensionActionRunner::OnRequestScriptInjectionPermission(
    const std::string& extension_id,
    UserScript::InjectionType script_type,
    UserScript::RunLocation run_location,
    int64_t request_id) {
  if (!crx_file::id_util::IdIsValid(extension_id)) {
    NOTREACHED() << "'" << extension_id << "' is not a valid id.";
    return;
  }

  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  // We shouldn't allow extensions which are no longer enabled to run any
  // scripts. Ignore the request.
  if (!extension)
    return;

  ++num_page_requests_;

  switch (RequiresUserConsentForScriptInjection(extension, script_type)) {
    case PermissionsData::ACCESS_ALLOWED:
      PermitScriptInjection(request_id);
      break;
    case PermissionsData::ACCESS_WITHHELD:
      // This base::Unretained() is safe, because the callback is only invoked
      // by this object.
      RequestScriptInjection(
          extension, run_location,
          base::Bind(&ExtensionActionRunner::PermitScriptInjection,
                     base::Unretained(this), request_id));
      break;
    case PermissionsData::ACCESS_DENIED:
      // We should usually only get a "deny access" if the page changed (as the
      // renderer wouldn't have requested permission if the answer was always
      // "no"). Just let the request fizzle and die.
      break;
  }
}

void ExtensionActionRunner::PermitScriptInjection(int64_t request_id) {
  // This only sends the response to the renderer - the process of adding the
  // extension to the list of |permitted_extensions_| is done elsewhere.
  // TODO(devlin): Instead of sending this to all frames, we should include the
  // routing_id in the permission request message, and send only to the proper
  // frame (sending it to all frames doesn't hurt, but isn't as efficient).
  web_contents()->SendToAllFrames(new ExtensionMsg_PermitScriptInjection(
      MSG_ROUTING_NONE,  // Routing id is set by the |web_contents|.
      request_id));
}

void ExtensionActionRunner::NotifyChange(const Extension* extension) {
  ExtensionActionAPI* extension_action_api =
      ExtensionActionAPI::Get(browser_context_);
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)
          ->GetExtensionAction(*extension);
  // If the extension has an action, we need to notify that it's updated.
  if (extension_action) {
    extension_action_api->NotifyChange(extension_action, web_contents(),
                                       browser_context_);
  }

  // We also notify that page actions may have changed.
  extension_action_api->NotifyPageActionsChanged(web_contents());
}

void ExtensionActionRunner::LogUMA() const {
  // We only log the permitted extensions metric if the feature was used at all
  // on the page, because otherwise the data will be boring.
  if (was_used_on_page_) {
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.PermittedExtensions",
        permitted_extensions_.size());
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.DeniedExtensions",
        pending_scripts_.size());
  }
}

void ExtensionActionRunner::ShowBlockedActionBubble(
    const Extension* extension) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  ToolbarActionsBar* toolbar_actions_bar =
      browser ? browser->window()->GetToolbarActionsBar() : nullptr;
  if (toolbar_actions_bar) {
    auto callback =
        base::Bind(&ExtensionActionRunner::OnBlockedActionBubbleClosed,
                   weak_factory_.GetWeakPtr(), extension->id());
    if (default_bubble_close_action_for_testing_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(callback, *default_bubble_close_action_for_testing_));
    } else {
      toolbar_actions_bar->ShowToolbarActionBubble(
          std::make_unique<BlockedActionBubbleDelegate>(callback,
                                                        extension->id()));
    }
  }
}

void ExtensionActionRunner::OnBlockedActionBubbleClosed(
    const std::string& extension_id,
    ToolbarActionsBarBubbleDelegate::CloseAction action) {
  // If the user agreed to refresh the page, do so.
  if (action == ToolbarActionsBarBubbleDelegate::CLOSE_EXECUTE) {
    const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                     ->enabled_extensions()
                                     .GetByID(extension_id);
    if (!extension)
      return;
    {
      // Ignore the active tab permission being granted because we don't want
      // to run scripts right before we refresh the page.
      base::AutoReset<bool> ignore_active_tab(&ignore_active_tab_granted_,
                                              true);
      TabHelper::FromWebContents(web_contents())
          ->active_tab_permission_granter()
          ->GrantIfRequested(extension);
    }
    web_contents()->GetController().Reload(content::ReloadType::NORMAL, false);
  }
}

bool ExtensionActionRunner::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionActionRunner, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestScriptInjectionPermission,
                        OnRequestScriptInjectionPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionActionRunner::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  LogUMA();
  num_page_requests_ = 0;
  permitted_extensions_.clear();
  pending_scripts_.clear();
  web_request_blocked_.clear();
  was_used_on_page_ = false;
  weak_factory_.InvalidateWeakPtrs();
}

void ExtensionActionRunner::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  PendingScriptMap::iterator iter = pending_scripts_.find(extension->id());
  if (iter != pending_scripts_.end()) {
    pending_scripts_.erase(iter);
    ExtensionActionAPI::Get(browser_context_)
        ->NotifyPageActionsChanged(web_contents());
  }
}

}  // namespace extensions
