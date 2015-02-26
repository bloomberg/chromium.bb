// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_H_
#define EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_H_

#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/api/management/management_api_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionRegistry;
class ExtensionUninstallDialog;
struct WebApplicationInfo;

namespace extensions {
class ExtensionRegistry;
class RequirementsChecker;

class ManagementFunction : public SyncExtensionFunction {
 protected:
  ~ManagementFunction() override {}
};

class AsyncManagementFunction : public AsyncExtensionFunction {
 protected:
  ~AsyncManagementFunction() override {}
};

class ManagementGetAllFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getAll", MANAGEMENT_GETALL)

 protected:
  ~ManagementGetAllFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ManagementGetFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.get", MANAGEMENT_GET)

 protected:
  ~ManagementGetFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ManagementGetSelfFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getSelf", MANAGEMENT_GETSELF)

 protected:
  ~ManagementGetSelfFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ManagementGetPermissionWarningsByIdFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getPermissionWarningsById",
                             MANAGEMENT_GETPERMISSIONWARNINGSBYID)

 protected:
  ~ManagementGetPermissionWarningsByIdFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ManagementGetPermissionWarningsByManifestFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getPermissionWarningsByManifest",
                             MANAGEMENT_GETPERMISSIONWARNINGSBYMANIFEST);

  // Called when utility process finishes.
  void OnParseSuccess(scoped_ptr<base::Value> value);
  void OnParseFailure(const std::string& error);

 protected:
  ~ManagementGetPermissionWarningsByManifestFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;
};

class ManagementLaunchAppFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.launchApp", MANAGEMENT_LAUNCHAPP)

 protected:
  ~ManagementLaunchAppFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

class ManagementSetEnabledFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setEnabled", MANAGEMENT_SETENABLED)

  ManagementSetEnabledFunction();

  void InstallUIProceed();
  void InstallUIAbort(bool user_initiated);

 protected:
  ~ManagementSetEnabledFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnRequirementsChecked(const std::vector<std::string>& requirements);

  std::string extension_id_;

  scoped_ptr<InstallPromptDelegate> install_prompt_;

  scoped_ptr<RequirementsChecker> requirements_checker_;
};

class ManagementUninstallFunctionBase : public UIThreadExtensionFunction {
 public:
  ManagementUninstallFunctionBase();

  static void SetAutoConfirmForTest(bool should_proceed);

  void ExtensionUninstallAccepted();
  void ExtensionUninstallCanceled();

 protected:
  ~ManagementUninstallFunctionBase() override;
  ResponseAction Uninstall(const std::string& extension_id,
                           bool show_confirm_dialog);

 private:
  // If should_uninstall is true, this method does the actual uninstall.
  // If |show_uninstall_dialog|, then this function will be called by one of the
  // Accepted/Canceled callbacks. Otherwise, it's called directly from RunAsync.
  void Finish(bool should_uninstall);

  std::string target_extension_id_;

  scoped_ptr<UninstallDialogDelegate> uninstall_dialog_;
};

class ManagementUninstallFunction : public ManagementUninstallFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstall", MANAGEMENT_UNINSTALL)
  ManagementUninstallFunction();

 private:
  ~ManagementUninstallFunction() override;
  ResponseAction Run() override;
};

class ManagementUninstallSelfFunction : public ManagementUninstallFunctionBase {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstallSelf",
                             MANAGEMENT_UNINSTALLSELF);
  ManagementUninstallSelfFunction();

 private:
  ~ManagementUninstallSelfFunction() override;
  ResponseAction Run() override;
};

class ManagementCreateAppShortcutFunction : public AsyncManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.createAppShortcut",
                             MANAGEMENT_CREATEAPPSHORTCUT);

  ManagementCreateAppShortcutFunction();

  void OnCloseShortcutPrompt(bool created);

  static void SetAutoConfirmForTest(bool should_proceed);

 protected:
  ~ManagementCreateAppShortcutFunction() override;

  bool RunAsync() override;
};

class ManagementSetLaunchTypeFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setLaunchType",
                             MANAGEMENT_SETLAUNCHTYPE);

 protected:
  ~ManagementSetLaunchTypeFunction() override {}

  bool RunSync() override;
};

class ManagementGenerateAppForLinkFunction : public AsyncManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.generateAppForLink",
                             MANAGEMENT_GENERATEAPPFORLINK);

  ManagementGenerateAppForLinkFunction();

  void FinishCreateBookmarkApp(const Extension* extension,
                               const WebApplicationInfo& web_app_info);

 protected:
  ~ManagementGenerateAppForLinkFunction() override;

  bool RunAsync() override;

 private:
  scoped_ptr<AppForLinkDelegate> app_for_link_delegate_;
};

class ManagementEventRouter : public ExtensionRegistryObserver {
 public:
  explicit ManagementEventRouter(content::BrowserContext* context);
  ~ManagementEventRouter() override;

 private:
  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

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
  ~ManagementAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ManagementAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Returns the ManagementAPI delegate.
  const ManagementAPIDelegate* GetDelegate() const { return delegate_.get(); }

 private:
  friend class BrowserContextKeyedAPIFactory<ManagementAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ManagementAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<ManagementEventRouter> management_event_router_;

  scoped_ptr<ManagementAPIDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagementAPI);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MANAGEMENT_MANAGEMENT_API_H_
