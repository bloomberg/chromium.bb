// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_

#include <vector>

#include "base/compiler_specific.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

class BrowserContextKeyedServiceFactory;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class DeclarativeUserScriptMaster;
class EventRouter;
class InfoMap;
class LazyBackgroundTaskQueue;
class ProcessManager;
class RendererStartupHelper;
class SharedUserScriptMaster;

// A simplified version of ExtensionSystem for app_shell. Allows
// app_shell to skip initialization of services it doesn't need.
class ShellExtensionSystem : public ExtensionSystem {
 public:
  explicit ShellExtensionSystem(content::BrowserContext* browser_context);
  virtual ~ShellExtensionSystem();

  // Loads an unpacked application from a directory. Returns true on success.
  bool LoadApp(const base::FilePath& app_dir);

  // Launch the currently loaded app.
  void LaunchApp();

  // KeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  scoped_refptr<Extension> extension() { return extension_; }

  // ExtensionSystem implementation:
  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE;
  virtual ExtensionService* extension_service() OVERRIDE;
  virtual RuntimeData* runtime_data() OVERRIDE;
  virtual ManagementPolicy* management_policy() OVERRIDE;
  virtual SharedUserScriptMaster* shared_user_script_master() OVERRIDE;
  virtual ProcessManager* process_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;
  virtual StateStore* rules_store() OVERRIDE;
  virtual InfoMap* info_map() OVERRIDE;
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue() OVERRIDE;
  virtual EventRouter* event_router() OVERRIDE;
  virtual ExtensionWarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;
  virtual ErrorConsole* error_console() OVERRIDE;
  virtual InstallVerifier* install_verifier() OVERRIDE;
  virtual QuotaService* quota_service() OVERRIDE;
  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) OVERRIDE;
  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual const OneShotEvent& ready() const OVERRIDE;
  virtual ContentVerifier* content_verifier() OVERRIDE;
  virtual scoped_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) OVERRIDE;
  virtual DeclarativeUserScriptMaster*
      GetDeclarativeUserScriptMasterByExtension(
          const ExtensionId& extension_id) OVERRIDE;

 private:
  content::BrowserContext* browser_context_;  // Not owned.

  // Extension ID for the app.
  std::string app_id_;

  scoped_refptr<Extension> extension_;

  // Data to be accessed on the IO thread. Must outlive process_manager_.
  scoped_refptr<InfoMap> info_map_;

  scoped_ptr<RuntimeData> runtime_data_;
  scoped_ptr<LazyBackgroundTaskQueue> lazy_background_task_queue_;
  scoped_ptr<EventRouter> event_router_;
  scoped_ptr<ProcessManager> process_manager_;
  scoped_ptr<QuotaService> quota_service_;

  // Signaled when the extension system has completed its startup tasks.
  OneShotEvent ready_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionSystem);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_H_
