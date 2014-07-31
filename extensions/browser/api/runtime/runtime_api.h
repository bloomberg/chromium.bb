// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_
#define EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_

#include <string>

#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
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

namespace core_api {
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
  virtual ~RuntimeAPI();

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void ReloadExtension(const std::string& extension_id);
  bool CheckForUpdates(const std::string& extension_id,
                       const RuntimeAPIDelegate::UpdateCheckCallback& callback);
  void OpenURL(const GURL& uninstall_url);
  bool GetPlatformInfo(core_api::runtime::PlatformInfo* info);
  bool RestartDevice(std::string* error_message);

 private:
  friend class BrowserContextKeyedAPIFactory<RuntimeAPI>;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionUninstalled(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UninstallReason reason) OVERRIDE;

  // BrowserContextKeyedAPI implementation:
  static const char* service_name() { return "RuntimeAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;
  virtual void Shutdown() OVERRIDE;

  // extensions::UpdateObserver overrides:
  virtual void OnAppUpdateAvailable(const Extension* extension) OVERRIDE;
  virtual void OnChromeUpdateAvailable() OVERRIDE;

  // ProcessManagerObserver implementation:
  virtual void OnBackgroundHostStartup(const Extension* extension) OVERRIDE;

  scoped_ptr<RuntimeAPIDelegate> delegate_;

  content::BrowserContext* browser_context_;

  // True if we should dispatch the chrome.runtime.onInstalled event with
  // reason "chrome_update" upon loading each extension.
  bool dispatch_chrome_updated_event_;

  content::NotificationRegistrar registrar_;

  // Listen to extension notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeAPI);
};

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
      core_api::runtime::OnRestartRequired::Reason reason);

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
  virtual ~RuntimeGetBackgroundPageFunction() {}
  virtual ResponseAction Run() OVERRIDE;

 private:
  void OnPageLoaded(ExtensionHost*);
};

class RuntimeSetUninstallURLFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.setUninstallURL", RUNTIME_SETUNINSTALLURL)

 protected:
  virtual ~RuntimeSetUninstallURLFunction() {}
  virtual ResponseAction Run() OVERRIDE;
};

class RuntimeReloadFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.reload", RUNTIME_RELOAD)

 protected:
  virtual ~RuntimeReloadFunction() {}
  virtual ResponseAction Run() OVERRIDE;
};

class RuntimeRequestUpdateCheckFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.requestUpdateCheck",
                             RUNTIME_REQUESTUPDATECHECK)

 protected:
  virtual ~RuntimeRequestUpdateCheckFunction() {}
  virtual ResponseAction Run() OVERRIDE;

 private:
  void CheckComplete(const RuntimeAPIDelegate::UpdateCheckResult& result);
};

class RuntimeRestartFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.restart", RUNTIME_RESTART)

 protected:
  virtual ~RuntimeRestartFunction() {}
  virtual ResponseAction Run() OVERRIDE;
};

class RuntimeGetPlatformInfoFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getPlatformInfo",
                             RUNTIME_GETPLATFORMINFO);

 protected:
  virtual ~RuntimeGetPlatformInfoFunction() {}
  virtual ResponseAction Run() OVERRIDE;
};

class RuntimeGetPackageDirectoryEntryFunction
    : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("runtime.getPackageDirectoryEntry",
                             RUNTIME_GETPACKAGEDIRECTORYENTRY)

 protected:
  virtual ~RuntimeGetPackageDirectoryEntryFunction() {}
  virtual ResponseAction Run() OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_RUNTIME_RUNTIME_API_H_
