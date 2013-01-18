// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionService;
class ExtensionUninstallDialog;

namespace extensions {

class ManagementFunction : public SyncExtensionFunction {
 protected:
  virtual ~ManagementFunction() {}

  ExtensionService* service();
};

class AsyncManagementFunction : public AsyncExtensionFunction {
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
  virtual bool RunImpl() OVERRIDE;
};

class ManagementGetFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.get", MANAGEMENT_GET)

 protected:
  virtual ~ManagementGetFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ManagementGetPermissionWarningsByIdFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.getPermissionWarningsById",
                             MANAGEMENT_GETPERMISSIONWARNINGSBYID)

 protected:
  virtual ~ManagementGetPermissionWarningsByIdFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ManagementGetPermissionWarningsByManifestFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "management.getPermissionWarningsByManifest",
      MANAGEMENT_GETPERMISSIONWARNINGSBYMANIFEST);

  // Called when utility process finishes.
  void OnParseSuccess(base::DictionaryValue* parsed_manifest);
  void OnParseFailure(const std::string& error);

 protected:
  virtual ~ManagementGetPermissionWarningsByManifestFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ManagementLaunchAppFunction : public ManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("management.launchApp", MANAGEMENT_LAUNCHAPP)

 protected:
  virtual ~ManagementLaunchAppFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ManagementSetEnabledFunction : public AsyncManagementFunction,
                           public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("management.setEnabled", MANAGEMENT_SETENABLED)

  ManagementSetEnabledFunction();

 protected:
  virtual ~ManagementSetEnabledFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // ExtensionInstallPrompt::Delegate.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

 private:
  std::string extension_id_;

  // Used for prompting to re-enable items with permissions escalation updates.
  scoped_ptr<ExtensionInstallPrompt> install_prompt_;
};

class ManagementUninstallFunction : public AsyncManagementFunction,
                          public ExtensionUninstallDialog::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("management.uninstall", MANAGEMENT_UNINSTALL)

  ManagementUninstallFunction();
  static void SetAutoConfirmForTest(bool should_proceed);

  // ExtensionUninstallDialog::Delegate implementation.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 private:
  virtual ~ManagementUninstallFunction();

  virtual bool RunImpl() OVERRIDE;

  // If should_uninstall is true, this method does the actual uninstall.
  // If |show_uninstall_dialog|, then this function will be called by one of the
  // Accepted/Canceled callbacks. Otherwise, it's called directly from RunImpl.
  void Finish(bool should_uninstall);

  std::string extension_id_;
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;
};

class ManagementEventRouter : public content::NotificationObserver {
 public:
  explicit ManagementEventRouter(Profile* profile);
  virtual ~ManagementEventRouter();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ManagementEventRouter);
};

class ManagementAPI : public ProfileKeyedAPI,
                      public extensions::EventRouter::Observer {
 public:
  explicit ManagementAPI(Profile* profile);
  virtual ~ManagementAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<ManagementAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<ManagementAPI>;

  Profile* profile_;

  // ProfileKeyedAPI implementation.
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
