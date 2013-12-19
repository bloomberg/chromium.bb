// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_SHELL_EXTENSION_SYSTEM_H_
#define APPS_SHELL_SHELL_EXTENSION_SYSTEM_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_system.h"
#include "extensions/common/one_shot_event.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class EventRouter;
class LazyBackgroundTaskQueue;
class ProcessManager;

// A simplified version of ExtensionSystem for app_shell. Allows
// app_shell to skip initialization of services it doesn't need.
class ShellExtensionSystem : public ExtensionSystem {
 public:
  explicit ShellExtensionSystem(content::BrowserContext* browser_context);
  virtual ~ShellExtensionSystem();

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // ExtensionSystem implementation:
  virtual void InitForRegularProfile(bool extensions_enabled) OVERRIDE;
  virtual ExtensionService* extension_service() OVERRIDE;
  virtual ManagementPolicy* management_policy() OVERRIDE;
  virtual UserScriptMaster* user_script_master() OVERRIDE;
  virtual ProcessManager* process_manager() OVERRIDE;
  virtual StateStore* state_store() OVERRIDE;
  virtual StateStore* rules_store() OVERRIDE;
  virtual InfoMap* info_map() OVERRIDE;
  virtual LazyBackgroundTaskQueue* lazy_background_task_queue()
      OVERRIDE;
  virtual EventRouter* event_router() OVERRIDE;
  virtual ExtensionWarningService* warning_service() OVERRIDE;
  virtual Blacklist* blacklist() OVERRIDE;
  virtual ErrorConsole* error_console() OVERRIDE;
  virtual InstallVerifier* install_verifier() OVERRIDE;
  virtual void RegisterExtensionWithRequestContexts(
      const Extension* extension) OVERRIDE;
  virtual void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual const OneShotEvent& ready() const OVERRIDE;

 private:
  content::BrowserContext* browser_context_;  // Not owned.

  scoped_ptr<LazyBackgroundTaskQueue> lazy_background_task_queue_;
  scoped_ptr<EventRouter> event_router_;
  scoped_ptr<ProcessManager> process_manager_;

  // Signaled when the extension system has completed its startup tasks.
  OneShotEvent ready_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionSystem);
};

}  // namespace extensions

#endif  // APPS_SHELL_SHELL_EXTENSION_SYSTEM_H_
