// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_

#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/common/web_application_info.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionService;
class ExtensionUninstallDialog;

namespace extensions {
class ExtensionRegistry;

class ManagementFunction : public ChromeSyncExtensionFunction {
 protected:
  virtual ~ManagementFunction() {}

  ExtensionService* service();
};

class AsyncManagementFunction : public ChromeAsyncExtensionFunction {
 protected:
  virtual ~AsyncManagementFunction() {}

  ExtensionService* service();
};

class ManagementGetAllFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getAll", MANAGEMENT_GETALL)

 protected:
  virtual ~ManagementGetAllFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ManagementGetFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.get", MANAGEMENT_GET)

 protected:
  virtual ~ManagementGetFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ManagementGetPermissionWarningsByIdFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getPermissionWarningsById",
                             MANAGEMENT_GETPERMISSIONWARNINGSBYID)

 protected:
  virtual ~ManagementGetPermissionWarningsByIdFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ManagementGetPermissionWarningsByManifestFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "management.getPermissionWarningsByManifest",
      MANAGEMENT_GETPERMISSIONWARNINGSBYMANIFEST);

  // Called when utility process finishes.
  void OnParseSuccess(scoped_ptr<base::DictionaryValue> parsed_manifest);
  void OnParseFailure(const std::string& error);

 protected:
  virtual ~ManagementGetPermissionWarningsByManifestFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

class ManagementLaunchAppFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.launchApp", MANAGEMENT_LAUNCHAPP)

 protected:
  virtual ~ManagementLaunchAppFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ManagementSetEnabledFunction : public AsyncManagementFunction,
                           public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setEnabled", MANAGEMENT_SETENABLED)

  ManagementSetEnabledFunction();

 protected:
  virtual ~ManagementSetEnabledFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  // ExtensionInstallPrompt::Delegate.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 private:
  std::string extension_id_;

  // Used for prompting to re-enable items with permissions escalation updates.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;
};

class ManagementUninstallFunctionBase : public AsyncManagementFunction,
                          public ExtensionUninstallDialog::Delegate {
 public:
  ManagementUninstallFunctionBase();

  static void SetAutoConfirmForTest(bool should_proceed);

  // ExtensionUninstallDialog::Delegate implementation.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 protected:
  virtual ~ManagementUninstallFunctionBase();

  bool Uninstall(const std::string& extension_id, bool show_confirm_dialog);
 private:

  // If should_uninstall is true, this method does the actual uninstall.
  // If |show_uninstall_dialog|, then this function will be called by one of the
  // Accepted/Canceled callbacks. Otherwise, it's called directly from RunAsync.
  void Finish(bool should_uninstall);

  std::string extension_id_;
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;
};

class ManagementUninstallFunction : public ManagementUninstallFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstall", MANAGEMENT_UNINSTALL)

  ManagementUninstallFunction();

 private:
  virtual ~ManagementUninstallFunction();

  virtual bool RunAsync() OVERRIDE;
};

class ManagementUninstallSelfFunction : public ManagementUninstallFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstallSelf",
      MANAGEMENT_UNINSTALLSELF);

  ManagementUninstallSelfFunction();

 private:
  virtual ~ManagementUninstallSelfFunction();

  virtual bool RunAsync() OVERRIDE;
};

class ManagementCreateAppShortcutFunction : public AsyncManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.createAppShortcut",
      MANAGEMENT_CREATEAPPSHORTCUT);

  ManagementCreateAppShortcutFunction();

  void OnCloseShortcutPrompt(bool created);

  static void SetAutoConfirmForTest(bool should_proceed);

 protected:
  virtual ~ManagementCreateAppShortcutFunction();

  virtual bool RunAsync() OVERRIDE;
};

class ManagementSetLaunchTypeFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setLaunchType",
      MANAGEMENT_SETLAUNCHTYPE);

 protected:
  virtual ~ManagementSetLaunchTypeFunction() {}

  virtual bool RunSync() OVERRIDE;
};

class ManagementGenerateAppForLinkFunction : public AsyncManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.generateAppForLink",
      MANAGEMENT_GENERATEAPPFORLINK);

  ManagementGenerateAppForLinkFunction();

 protected:
  virtual ~ManagementGenerateAppForLinkFunction();

  virtual bool RunAsync() OVERRIDE;

 private:
  void OnFaviconForApp(const favicon_base::FaviconImageResult& image_result);
  void FinishCreateBookmarkApp(const Extension* extension,
                               const WebApplicationInfo& web_app_info);

  std::string title_;
  GURL launch_url_;

  scoped_ptr<BookmarkAppHelper> bookmark_app_helper_;

  // Used for favicon loading tasks.
  base::CancelableTaskTracker cancelable_task_tracker_;
};

class ManagementEventRouter : public ExtensionRegistryObserver {
 public:
  explicit ManagementEventRouter(content::BrowserContext* context);
  virtual ~ManagementEventRouter();

 private:
  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionInstalled(content::BrowserContext* browser_context,
                                    const Extension* extension,
                                    bool is_update) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // Dispatches management api events to listening extensions.
  void BroadcastEvent(const Extension* extension, const char* event_name);

  content::BrowserContext* browser_context_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ManagementEventRouter);
};

class ManagementAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer {
 public:
  explicit ManagementAPI(content::BrowserContext* context);
  virtual ~ManagementAPI();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ManagementAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<ManagementAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "ManagementAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<ManagementEventRouter> management_event_router_;

  DISALLOW_COPY_AND_ASSIGN(ManagementAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_
