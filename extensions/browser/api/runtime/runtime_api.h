// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_
#define EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_

#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/update_observer.h"
#include "extensions/common/api/runtime.h"

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

namespace extensions {

namespace api {
namespace runtime {
struct PlatformInfo;
}
}

class Extension;
class ExtensionHost;
class ExtensionRegistry;

// Runtime API dispatches onStartup, onInstalled, and similar events to
// extensions. There is one instance shared between a browser context and
// its related incognito instance.
class RuntimeAPI : public BrowserContextKeyedAPI,
                   public content::NotificationObserver,
                   public ExtensionRegistryObserver,
                   public UpdateObserver,
                   public ProcessManagerObserver {
 public:
  static BrowserContextKeyedAPIFactory<RuntimeAPI>* GetFactoryInstance();

  explicit RuntimeAPI(content::BrowserContext* context);
  ~RuntimeAPI() override;

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void ReloadExtension(const std::string& extension_id);
  bool CheckForUpdates(const std::string& extension_id,
                       const RuntimeAPIDelegate::UpdateCheckCallback& callback);
  void OpenURL(const GURL& uninstall_url);
  bool GetPlatformInfo(api::runtime::PlatformInfo* info);
  bool RestartDevice(std::string* error_message);
  bool OpenOptionsPage(const Extension* extension);

 private:
  friend class BrowserContextKeyedAPIFactory<RuntimeAPI>;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override;

  // BrowserContextKeyedAPI implementation:
  static const char* service_name() { return "RuntimeAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;
  void Shutdown() override;

  // extensions::UpdateObserver overrides:
  void OnAppUpdateAvailable(const Extension* extension) override;
  void OnChromeUpdateAvailable() override;

  // ProcessManagerObserver implementation:
  void OnBackgroundHostStartup(const Extension* extension) override;

  // Pref related functions that deals with info about installed extensions that
  // has not been loaded yet.
  // Used to send chrome.runtime.onInstalled event upon loading the extensions.
  bool ReadPendingOnInstallInfoFromPref(const ExtensionId& extension_id,
                                        base::Version* previous_version);
  void RemovePendingOnInstallInfoFromPref(const ExtensionId& extension_id);
  void StorePendingOnInstallInfoToPref(const Extension* extension);

  std::unique_ptr<RuntimeAPIDelegate> delegate_;

  content::BrowserContext* browser_context_;

  // True if we should dispatch the chrome.runtime.onInstalled event with
  // reason "chrome_update" upon loading each extension.
  bool dispatch_chrome_updated_event_;

  content::NotificationRegistrar registrar_;

  // Listen to extension notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeAPI);
};

template <>
void BrowserContextKeyedAPIFactory<RuntimeAPI>::DeclareFactoryDependencies();

class RuntimeEventRouter {
 public:
  // Dispatches the onStartup event to all currently-loaded extensions.
  static void DispatchOnStartupEvent(content::BrowserContext* context,
                                     const std::string& extension_id);

  // Dispatches the onInstalled event to the given extension.
  static void DispatchOnInstalledEvent(content::BrowserContext* context,
                                       const std::string& extension_id,
                                       const base::Version& old_version,
                                       bool chrome_updated);

  // Dispatches the onUpdateAvailable event to the given extension.
  static void DispatchOnUpdateAvailableEvent(
      content::BrowserContext* context,
      const std::string& extension_id,
      const base::DictionaryValue* manifest);

  // Dispatches the onBrowserUpdateAvailable event to all extensions.
  static void DispatchOnBrowserUpdateAvailableEvent(
      content::BrowserContext* context);

  // Dispatches the onRestartRequired event to the given app.
  static void DispatchOnRestartRequiredEvent(
      content::BrowserContext* context,
      const std::string& app_id,
      api::runtime::OnRestartRequiredReason reason);

  // Does any work needed at extension uninstall (e.g. load uninstall url).
  static void OnExtensionUninstalled(content::BrowserContext* context,
                                     const std::string& extension_id,
                                     UninstallReason reason);
};

class RuntimeGetBackgroundPageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getBackgroundPage",
                             RUNTIME_GETBACKGROUNDPAGE)

 protected:
  ~RuntimeGetBackgroundPageFunction() override {}
  ResponseAction Run() override;

 private:
  void OnPageLoaded(ExtensionHost*);
};

class RuntimeOpenOptionsPageFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.openOptionsPage", RUNTIME_OPENOPTIONSPAGE)

 protected:
  ~RuntimeOpenOptionsPageFunction() override {}
  ResponseAction Run() override;
};

class RuntimeSetUninstallURLFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.setUninstallURL", RUNTIME_SETUNINSTALLURL)

 protected:
  ~RuntimeSetUninstallURLFunction() override {}
  ResponseAction Run() override;
};

class RuntimeReloadFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.reload", RUNTIME_RELOAD)

 protected:
  ~RuntimeReloadFunction() override {}
  ResponseAction Run() override;
};

class RuntimeRequestUpdateCheckFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.requestUpdateCheck",
                             RUNTIME_REQUESTUPDATECHECK)

 protected:
  ~RuntimeRequestUpdateCheckFunction() override {}
  ResponseAction Run() override;

 private:
  void CheckComplete(const RuntimeAPIDelegate::UpdateCheckResult& result);
};

class RuntimeRestartFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.restart", RUNTIME_RESTART)

 protected:
  ~RuntimeRestartFunction() override {}
  ResponseAction Run() override;
};

class RuntimeGetPlatformInfoFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getPlatformInfo",
                             RUNTIME_GETPLATFORMINFO);

 protected:
  ~RuntimeGetPlatformInfoFunction() override {}
  ResponseAction Run() override;
};

class RuntimeGetPackageDirectoryEntryFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getPackageDirectoryEntry",
                             RUNTIME_GETPACKAGEDIRECTORYENTRY)

 protected:
  ~RuntimeGetPackageDirectoryEntryFunction() override {}
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_
