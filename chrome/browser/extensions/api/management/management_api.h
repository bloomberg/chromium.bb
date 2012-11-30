// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class ExtensionService;
class ExtensionUninstallDialog;

class ExtensionManagementFunction : public SyncExtensionFunction {
 protected:
  virtual ~ExtensionManagementFunction() {}

  ExtensionService* service();
};

class AsyncExtensionManagementFunction : public AsyncExtensionFunction {
 protected:
  virtual ~AsyncExtensionManagementFunction() {}

  ExtensionService* service();
};

class GetAllExtensionsFunction : public ExtensionManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("management.getAll");

 protected:
  virtual ~GetAllExtensionsFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetExtensionByIdFunction : public ExtensionManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("management.get");

 protected:
  virtual ~GetExtensionByIdFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetPermissionWarningsByIdFunction : public ExtensionManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("management.getPermissionWarningsById");

 protected:
  virtual ~GetPermissionWarningsByIdFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class GetPermissionWarningsByManifestFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "management.getPermissionWarningsByManifest");

  // Called when utility process finishes.
  void OnParseSuccess(base::DictionaryValue* parsed_manifest);
  void OnParseFailure(const std::string& error);

 protected:
  virtual ~GetPermissionWarningsByManifestFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class LaunchAppFunction : public ExtensionManagementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("management.launchApp");

 protected:
  virtual ~LaunchAppFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class SetEnabledFunction : public AsyncExtensionManagementFunction,
                           public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("management.setEnabled");

  SetEnabledFunction();

 protected:
  virtual ~SetEnabledFunction();

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

class UninstallFunction : public AsyncExtensionManagementFunction,
                          public ExtensionUninstallDialog::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("management.uninstall");

  UninstallFunction();
  static void SetAutoConfirmForTest(bool should_proceed);

  // ExtensionUninstallDialog::Delegate implementation.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 private:
  virtual ~UninstallFunction();

  virtual bool RunImpl() OVERRIDE;

  // If should_uninstall is true, this method does the actual uninstall.
  // If |show_uninstall_dialog|, then this function will be called by one of the
  // Accepted/Canceled callbacks. Otherwise, it's called directly from RunImpl.
  void Finish(bool should_uninstall);

  std::string extension_id_;
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;
};

class ExtensionManagementEventRouter : public content::NotificationObserver {
 public:
  explicit ExtensionManagementEventRouter(Profile* profile);
  virtual ~ExtensionManagementEventRouter();

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagementEventRouter);
};

class ExtensionManagementAPI : public ProfileKeyedService,
                               public extensions::EventRouter::Observer {
 public:
  explicit ExtensionManagementAPI(Profile* profile);
  virtual ~ExtensionManagementAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const extensions::EventListenerInfo& details)
      OVERRIDE;

 private:
  Profile* profile_;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<ExtensionManagementEventRouter> management_event_router_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_MANAGEMENT_MANAGEMENT_API_H_
