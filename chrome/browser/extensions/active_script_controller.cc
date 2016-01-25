// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/active_script_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/active_tab_permission_granter.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message_macros.h"

namespace extensions {

ActiveScriptController::ActiveScriptController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      num_page_requests_(0),
      browser_context_(web_contents->GetBrowserContext()),
      was_used_on_page_(false),
      extension_registry_observer_(this) {
  CHECK(web_contents);
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

ActiveScriptController::~ActiveScriptController() {
  LogUMA();
}

// static
ActiveScriptController* ActiveScriptController::GetForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents)
    return NULL;
  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  return tab_helper ? tab_helper->active_script_controller() : NULL;
}

void ActiveScriptController::OnActiveTabPermissionGranted(
    const Extension* extension) {
  RunPendingForExtension(extension);
}

void ActiveScriptController::OnClicked(const Extension* extension) {
  DCHECK(ContainsKey(pending_requests_, extension->id()));
  RunPendingForExtension(extension);
}

bool ActiveScriptController::WantsToRun(const Extension* extension) {
  return pending_requests_.count(extension->id()) > 0;
}

PermissionsData::AccessType
ActiveScriptController::RequiresUserConsentForScriptInjection(
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

void ActiveScriptController::RequestScriptInjection(
    const Extension* extension,
    const base::Closure& callback) {
  CHECK(extension);
  PendingRequestList& list = pending_requests_[extension->id()];
  list.push_back(callback);

  // If this was the first entry, we need to notify that a new extension wants
  // to run.
  if (list.size() == 1u)
    NotifyChange(extension);

  was_used_on_page_ = true;
}

void ActiveScriptController::RunPendingForExtension(
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

  PendingRequestMap::iterator iter = pending_requests_.find(extension->id());
  if (iter == pending_requests_.end())
    return;

  PendingRequestList requests;
  iter->second.swap(requests);
  pending_requests_.erase(extension->id());

  // Clicking to run the extension counts as granting it permission to run on
  // the given tab.
  // The extension may already have active tab at this point, but granting
  // it twice is essentially a no-op.
  TabHelper::FromWebContents(web_contents())->
      active_tab_permission_granter()->GrantIfRequested(extension);

  // Run all pending injections for the given extension.
  for (PendingRequestList::iterator request = requests.begin();
       request != requests.end();
       ++request) {
    request->Run();
  }

  // The extension ran, so we need to update the ExtensionActionAPI that we no
  // longer want to act.
  NotifyChange(extension);
}

void ActiveScriptController::OnRequestScriptInjectionPermission(
    const std::string& extension_id,
    UserScript::InjectionType script_type,
    int64_t request_id) {
  if (!crx_file::id_util::IdIsValid(extension_id)) {
    NOTREACHED() << "'" << extension_id << "' is not a valid id.";
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(browser_context_)
          ->enabled_extensions().GetByID(extension_id);
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
          extension,
          base::Bind(&ActiveScriptController::PermitScriptInjection,
                     base::Unretained(this),
                     request_id));
      break;
    case PermissionsData::ACCESS_DENIED:
      // We should usually only get a "deny access" if the page changed (as the
      // renderer wouldn't have requested permission if the answer was always
      // "no"). Just let the request fizzle and die.
      break;
  }
}

void ActiveScriptController::PermitScriptInjection(int64_t request_id) {
  // This only sends the response to the renderer - the process of adding the
  // extension to the list of |permitted_extensions_| is done elsewhere.
  // TODO(devlin): Instead of sending this to all frames, we should include the
  // routing_id in the permission request message, and send only to the proper
  // frame (sending it to all frames doesn't hurt, but isn't as efficient).
  web_contents()->SendToAllFrames(new ExtensionMsg_PermitScriptInjection(
      MSG_ROUTING_NONE,  // Routing id is set by the |web_contents|.
      request_id));
}

void ActiveScriptController::NotifyChange(const Extension* extension) {
  ExtensionActionAPI* extension_action_api =
      ExtensionActionAPI::Get(browser_context_);
  ExtensionAction* extension_action =
      ExtensionActionManager::Get(browser_context_)->
          GetExtensionAction(*extension);
  // If the extension has an action, we need to notify that it's updated.
  if (extension_action) {
    extension_action_api->NotifyChange(
        extension_action, web_contents(), browser_context_);
  }

  // We also notify that page actions may have changed.
  extension_action_api->NotifyPageActionsChanged(web_contents());
}

void ActiveScriptController::LogUMA() const {
  // We only log the permitted extensions metric if the feature was used at all
  // on the page, because otherwise the data will be boring.
  if (was_used_on_page_) {
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.PermittedExtensions",
        permitted_extensions_.size());
    UMA_HISTOGRAM_COUNTS_100(
        "Extensions.ActiveScriptController.DeniedExtensions",
        pending_requests_.size());
  }
}

bool ActiveScriptController::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ActiveScriptController, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RequestScriptInjectionPermission,
                        OnRequestScriptInjectionPermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ActiveScriptController::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  LogUMA();
  num_page_requests_ = 0;
  permitted_extensions_.clear();
  pending_requests_.clear();
  was_used_on_page_ = false;
}

void ActiveScriptController::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  PendingRequestMap::iterator iter = pending_requests_.find(extension->id());
  if (iter != pending_requests_.end()) {
    pending_requests_.erase(iter);
    ExtensionActionAPI::Get(browser_context_)->
        NotifyPageActionsChanged(web_contents());
  }
}

}  // namespace extensions
