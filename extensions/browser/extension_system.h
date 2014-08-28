// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_SYSTEM_H_
#define EXTENSIONS_BROWSER_EXTENSION_SYSTEM_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension.h"

class ExtensionService;

#if defined(OS_CHROMEOS)
namespace chromeos {
class DeviceLocalAccountManagementPolicyProvider;
}
#endif  // defined(OS_CHROMEOS)

namespace content {
class BrowserContext;
}

namespace extensions {

class Blacklist;
class ContentVerifier;
class DeclarativeUserScriptMaster;
class ErrorConsole;
class EventRouter;
class Extension;
class ExtensionSet;
class InfoMap;
class InstallVerifier;
class LazyBackgroundTaskQueue;
class ManagementPolicy;
class OneShotEvent;
class ProcessManager;
class QuotaService;
class RuntimeData;
class SharedUserScriptMaster;
class StateStore;
class WarningService;

// ExtensionSystem manages the lifetime of many of the services used by the
// extensions and apps system, and it handles startup and shutdown as needed.
// Eventually, we'd like to make more of these services into KeyedServices in
// their own right.
class ExtensionSystem : public KeyedService {
 public:
  ExtensionSystem();
  virtual ~ExtensionSystem();

  // Returns the instance for the given browser context, or NULL if none.
  static ExtensionSystem* Get(content::BrowserContext* context);

  // Initializes extensions machinery.
  // Component extensions are always enabled, external and user extensions are
  // controlled by |extensions_enabled|.
  virtual void InitForRegularProfile(bool extensions_enabled) = 0;

  // The ExtensionService is created at startup.
  virtual ExtensionService* extension_service() = 0;

  // Per-extension data that can change during the life of the process but
  // does not persist across restarts. Lives on UI thread. Created at startup.
  virtual RuntimeData* runtime_data() = 0;

  // The class controlling whether users are permitted to perform certain
  // actions on extensions (install, uninstall, disable, etc.).
  // The ManagementPolicy is created at startup.
  virtual ManagementPolicy* management_policy() = 0;

  // The SharedUserScriptMaster is created at startup.
  virtual SharedUserScriptMaster* shared_user_script_master() = 0;

  // The ProcessManager is created at startup.
  virtual ProcessManager* process_manager() = 0;

  // The StateStore is created at startup.
  virtual StateStore* state_store() = 0;

  // The rules store is created at startup.
  virtual StateStore* rules_store() = 0;

  // Returns the IO-thread-accessible extension data.
  virtual InfoMap* info_map() = 0;

  // The LazyBackgroundTaskQueue is created at startup.
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() = 0;

  // The EventRouter is created at startup.
  virtual EventRouter* event_router() = 0;

  // The WarningService is created at startup.
  virtual WarningService* warning_service() = 0;

  // The blacklist is created at startup.
  virtual Blacklist* blacklist() = 0;

  // The ErrorConsole is created at startup.
  virtual ErrorConsole* error_console() = 0;

  // The InstallVerifier is created at startup.
  virtual InstallVerifier* install_verifier() = 0;

  // Returns the QuotaService that limits calls to certain extension functions.
  // Lives on the UI thread. Created at startup.
  virtual QuotaService* quota_service() = 0;

  // Called by the ExtensionService that lives in this system. Gives the
  // info map a chance to react to the load event before the EXTENSION_LOADED
  // notification has fired. The purpose for handling this event first is to
  // avoid race conditions by making sure URLRequestContexts learn about new
  // extensions before anything else needs them to know.
  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) {}

  // Called by the ExtensionService that lives in this system. Lets the
  // info map clean up its RequestContexts once all the listeners to the
  // EXTENSION_UNLOADED notification have finished running.
  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) {}

  // Signaled when the extension system has completed its startup tasks.
  virtual const OneShotEvent& ready() const = 0;

  // Returns the content verifier, if any.
  virtual ContentVerifier* content_verifier() = 0;

  // Get a set of extensions that depend on the given extension.
  // TODO(elijahtaylor): Move SharedModuleService out of chrome/browser
  // so it can be retrieved from ExtensionSystem directly.
  virtual scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) = 0;

  // Get the user script master for declarative scripts, if any.
  virtual DeclarativeUserScriptMaster*
      GetDeclarativeUserScriptMasterByExtension(
          const ExtensionId& extension_id) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_SYSTEM_H_
