// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/blocked_action_type.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace IPC {
class Message;
}

namespace extensions {
class Extension;
class ExtensionRegistry;

// The provider for ExtensionActions corresponding to scripts which are actively
// running or need permission.
class ExtensionActionRunner : public content::WebContentsObserver,
                              public ExtensionRegistryObserver {
 public:
  explicit ExtensionActionRunner(content::WebContents* web_contents);
  ~ExtensionActionRunner() override;

  // Returns the ExtensionActionRunner for the given |web_contents|, or null
  // if one does not exist.
  static ExtensionActionRunner* GetForWebContents(
      content::WebContents* web_contents);

  // Executes the action for the given |extension| and returns any further
  // action (like showing a popup) that should be taken. If
  // |grant_tab_permissions| is true, this will also grant activeTab to the
  // extension (so this should only be done if this is through a direct user
  // action).
  ExtensionAction::ShowAction RunAction(const Extension* extension,
                                        bool grant_tab_permissions);

  // Runs any actions that were blocked for the given |extension|. As a
  // requirement, this will grant activeTab permission to the extension.
  void RunBlockedActions(const Extension* extension);

  // Notifies the ExtensionActionRunner that an extension has been granted
  // active tab permissions. This will run any pending injections for that
  // extension.
  void OnActiveTabPermissionGranted(const Extension* extension);

  // Called when a webRequest event for the given |extension| was blocked.
  void OnWebRequestBlocked(const Extension* extension);

  // Returns a bitmask of BlockedActionType for the actions that have been
  // blocked for the given extension.
  int GetBlockedActions(const Extension* extension);

  // Returns true if the given |extension| has any blocked actions.
  bool WantsToRun(const Extension* extension);

  int num_page_requests() const { return num_page_requests_; }

#if defined(UNIT_TEST)
  // Only used in tests.
  PermissionsData::AccessType RequiresUserConsentForScriptInjectionForTesting(
      const Extension* extension,
      UserScript::InjectionType type) {
    return RequiresUserConsentForScriptInjection(extension, type);
  }
  void RequestScriptInjectionForTesting(const Extension* extension,
                                        UserScript::RunLocation run_location,
                                        const base::Closure& callback) {
    return RequestScriptInjection(extension, run_location, callback);
  }
#endif  // defined(UNIT_TEST)

 private:
  struct PendingScript {
    PendingScript(UserScript::RunLocation run_location,
                  const base::Closure& permit_script);
    PendingScript(const PendingScript& other);
    ~PendingScript();

    // The run location that the script wants to inject at.
    UserScript::RunLocation run_location;

    // The callback to run when the script is permitted by the user.
    base::Closure permit_script;
  };

  using PendingScriptList = std::vector<PendingScript>;
  using PendingScriptMap = std::map<std::string, PendingScriptList>;

  // Returns true if the extension requesting script injection requires
  // user consent. If this is true, the caller should then register a request
  // via RequestScriptInjection().
  PermissionsData::AccessType RequiresUserConsentForScriptInjection(
      const Extension* extension,
      UserScript::InjectionType type);

  // |callback|. The only assumption that can be made about when (or if)
  // |callback| is run is that, if it is run, it will run on the current page.
  void RequestScriptInjection(const Extension* extension,
                              UserScript::RunLocation run_location,
                              const base::Closure& callback);

  // Runs any pending injections for the corresponding extension.
  void RunPendingScriptsForExtension(const Extension* extension);

  // Handle the RequestScriptInjectionPermission message.
  void OnRequestScriptInjectionPermission(const std::string& extension_id,
                                          UserScript::InjectionType script_type,
                                          UserScript::RunLocation run_location,
                                          int64_t request_id);

  // Grants permission for the given request to run.
  void PermitScriptInjection(int64_t request_id);

  // Notifies the ExtensionActionAPI of a change (either that an extension now
  // wants permission to run, or that it has been run).
  void NotifyChange(const Extension* extension);

  // Log metrics.
  void LogUMA() const;

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // The total number of requests from the renderer on the current page,
  // including any that are pending or were immediately granted.
  // Right now, used only in tests.
  int num_page_requests_;

  // The associated browser context.
  content::BrowserContext* browser_context_;

  // Whether or not the feature was used for any extensions. This may not be the
  // case if the user never enabled the scripts-require-action flag.
  bool was_used_on_page_;

  // The map of extension_id:pending_request of all pending script requests.
  PendingScriptMap pending_scripts_;

  // A set of ids for which the webRequest API was blocked on the page.
  std::set<std::string> web_request_blocked_;

  // The extensions which have been granted permission to run on the given page.
  // TODO(rdevlin.cronin): Right now, this just keeps track of extensions that
  // have been permitted to run on the page via this interface. Instead, it
  // should incorporate more fully with ActiveTab.
  std::set<std::string> permitted_extensions_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionRunner);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_RUNNER_H_
