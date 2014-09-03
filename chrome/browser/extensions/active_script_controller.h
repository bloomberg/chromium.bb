// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVE_SCRIPT_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVE_SCRIPT_CONTROLLER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace content {
class WebContents;
}

namespace IPC {
class Message;
}

class ExtensionAction;

namespace extensions {
class Extension;
class ExtensionRegistry;

// The provider for ExtensionActions corresponding to scripts which are actively
// running or need permission.
// TODO(rdevlin.cronin): This isn't really a controller, but it has good parity
// with LocationBar"Controller".
class ActiveScriptController : public content::WebContentsObserver,
                               public ExtensionRegistryObserver {
 public:
  explicit ActiveScriptController(content::WebContents* web_contents);
  virtual ~ActiveScriptController();

  // Returns the ActiveScriptController for the given |web_contents|, or NULL
  // if one does not exist.
  static ActiveScriptController* GetForWebContents(
      content::WebContents* web_contents);

  // Notifies the ActiveScriptController that an extension has been granted
  // active tab permissions. This will run any pending injections for that
  // extension.
  void OnActiveTabPermissionGranted(const Extension* extension);

  // Notifies the ActiveScriptController of detected ad injection.
  void OnAdInjectionDetected(const std::set<std::string>& ad_injectors);

  // Adds the visible origin to |extension|'s active permissions, granting
  // |extension| permission to always run script injections on the origin.
  void AlwaysRunOnVisibleOrigin(const Extension* extension);

  // Notifies the ActiveScriptController that the action for |extension| has
  // been clicked, running any pending tasks that were previously shelved.
  void OnClicked(const Extension* extension);

  // Returns true if the given |extension| has a pending script that wants to
  // run.
  bool WantsToRun(const Extension* extension);

#if defined(UNIT_TEST)
  // Only used in tests.
  PermissionsData::AccessType RequiresUserConsentForScriptInjectionForTesting(
      const Extension* extension,
      UserScript::InjectionType type) {
    return RequiresUserConsentForScriptInjection(extension, type);
  }
  void RequestScriptInjectionForTesting(const Extension* extension,
                                        const base::Closure& callback) {
    return RequestScriptInjection(extension, callback);
  }
#endif  // defined(UNIT_TEST)

 private:
  typedef std::vector<base::Closure> PendingRequestList;
  typedef std::map<std::string, PendingRequestList> PendingRequestMap;

  // Returns true if the extension requesting script injection requires
  // user consent. If this is true, the caller should then register a request
  // via RequestScriptInjection().
  PermissionsData::AccessType RequiresUserConsentForScriptInjection(
      const Extension* extension,
      UserScript::InjectionType type);

  // |callback|. The only assumption that can be made about when (or if)
  // |callback| is run is that, if it is run, it will run on the current page.
  void RequestScriptInjection(const Extension* extension,
                              const base::Closure& callback);

  // Runs any pending injections for the corresponding extension.
  void RunPendingForExtension(const Extension* extension);

  // Handle the RequestScriptInjectionPermission message.
  void OnRequestScriptInjectionPermission(
      const std::string& extension_id,
      UserScript::InjectionType script_type,
      int64 request_id);

  // Grants permission for the given request to run.
  void PermitScriptInjection(int64 request_id);

  // Log metrics.
  void LogUMA() const;

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // ExtensionRegistryObserver:
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // Whether or not the ActiveScriptController is enabled (corresponding to the
  // kActiveScriptEnforcement switch). If it is not, it acts as an empty shell,
  // always allowing scripts to run and never displaying actions.
  bool enabled_;

  // The map of extension_id:pending_request of all pending requests.
  PendingRequestMap pending_requests_;

  // The extensions which have been granted permission to run on the given page.
  // TODO(rdevlin.cronin): Right now, this just keeps track of extensions that
  // have been permitted to run on the page via this interface. Instead, it
  // should incorporate more fully with ActiveTab.
  std::set<std::string> permitted_extensions_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ActiveScriptController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVE_SCRIPT_CONTROLLER_H_
