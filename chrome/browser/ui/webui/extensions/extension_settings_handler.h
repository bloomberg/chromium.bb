// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/warning_service.h"
#include "url/gurl.h"

class ExtensionService;

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

namespace content {
class WebUIDataSource;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {
class Extension;
class ExtensionRegistry;
class ManagementPolicy;

// Information about a page running in an extension, for example a popup bubble,
// a background page, or a tab contents.
struct ExtensionPage {
  ExtensionPage(const GURL& url,
                int render_process_id,
                int render_view_id,
                bool incognito,
                bool generated_background_page);
  GURL url;
  int render_process_id;
  int render_view_id;
  bool incognito;
  bool generated_background_page;
};

// Extension Settings UI handler.
class ExtensionSettingsHandler
    : public content::WebUIMessageHandler,
      public content::NotificationObserver,
      public content::WebContentsObserver,
      public ErrorConsole::Observer,
      public ExtensionInstallPrompt::Delegate,
      public ExtensionManagement::Observer,
      public ExtensionPrefsObserver,
      public ExtensionRegistryObserver,
      public WarningService::Observer,
      public base::SupportsWeakPtr<ExtensionSettingsHandler> {
 public:
  ExtensionSettingsHandler();
  ~ExtensionSettingsHandler() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Extension Detail JSON Struct for page. |pages| is injected for unit
  // testing.
  // Note: |warning_service| can be NULL in unit tests.
  base::DictionaryValue* CreateExtensionDetailValue(
      const Extension* extension,
      const std::vector<ExtensionPage>& pages,
      const WarningService* warning_service);

  void GetLocalizedValues(content::WebUIDataSource* source);

 private:
  friend class ExtensionUITest;
  friend class BrokerDelegate;

  // content::WebContentsObserver implementation.
  void RenderViewDeleted(content::RenderViewHost* render_view_host) override;
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;

  // Allows injection for testing by friend classes.
  ExtensionSettingsHandler(ExtensionService* service,
                           ManagementPolicy* policy);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // ErrorConsole::Observer implementation.
  void OnErrorAdded(const ExtensionError* error) override;

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // ExtensionPrefsObserver implementation.
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disable_reasons) override;

  // ExtensionManagement::Observer implementation.
  void OnExtensionManagementSettingsChanged() override;

  // WarningService::Observer implementation.
  void ExtensionWarningsChanged() override;

  // ExtensionInstallPrompt::Delegate implementation.
  void InstallUIProceed() override;
  void InstallUIAbort(bool user_initiated) override;

  // Called after the App Info Dialog has closed.
  virtual void AppInfoDialogClosed();

  // Helper method that reloads all unpacked extensions.
  void ReloadUnpackedExtensions();

  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const base::ListValue* args);

  // Callback for "toggleDeveloperMode" message.
  void HandleToggleDeveloperMode(const base::ListValue* args);

  // Callback for "launch" message.
  void HandleLaunchMessage(const base::ListValue* args);

  // Callback for "repair" message.
  void HandleRepairMessage(const base::ListValue* args);

  // Callback for "enableIncognito" message.
  void HandleEnableIncognitoMessage(const base::ListValue* args);

  // Callback for "enableErrorCollection" message.
  void HandleEnableErrorCollectionMessage(const base::ListValue* args);

  // Callback for "allowOnAllUrls" message.
  void HandleAllowOnAllUrlsMessage(const base::ListValue* args);

  // Callback for "options" message.
  void HandleOptionsMessage(const base::ListValue* args);

  // Callback for "permissions" message.
  void HandlePermissionsMessage(const base::ListValue* args);

  // Callback for "showButton" message.
  void HandleShowButtonMessage(const base::ListValue* args);

  // Callback for "autoupdate" message.
  void HandleAutoUpdateMessage(const base::ListValue* args);

  // Callback for the "dismissADTPromo" message.
  void HandleDismissADTPromoMessage(const base::ListValue* args);

  // Callback for the "showPath" message.
  void HandleShowPath(const base::ListValue* args);

  // Utility for calling JavaScript window.alert in the page.
  void ShowAlert(const std::string& message);

  // Utility for callbacks that get an extension ID as the sole argument.
  // Returns NULL if the extension isn't active.
  const Extension* GetActiveExtension(const base::ListValue* args);

  // Forces a UI update if appropriate after a notification is received.
  void MaybeUpdateAfterNotification();

  // Register for notifications that we need to reload the page.
  void MaybeRegisterForNotifications();

  // Helper that lists the current inspectable html pages for an extension.
  std::vector<ExtensionPage> GetInspectablePagesForExtension(
      const Extension* extension, bool extension_is_enabled);
  void GetInspectablePagesForExtensionProcess(
      const Extension* extension,
      const std::set<content::RenderViewHost*>& views,
      std::vector<ExtensionPage>* result);
  void GetAppWindowPagesForExtensionProfile(const Extension* extension,
                                            Profile* profile,
                                            std::vector<ExtensionPage>* result);

  // Called when the reinstallation is complete.
  void OnReinstallComplete(bool success,
                           const std::string& error,
                           webstore_install::Result result);

  // Handles the load retry notification sent from
  // ExtensionService::ReportExtensionLoadError. Attempts to retry loading
  // extension from |path| if retry is true, otherwise removes |path| from the
  // vector of currently loading extensions.
  //
  // Does nothing if |path| is not a currently loading extension this object is
  // tracking.
  void HandleLoadRetryMessage(bool retry, const base::FilePath& path);

  // Our model.  Outlives us since it's owned by our containing profile.
  ExtensionService* extension_service_;

  // A convenience member, filled once the extension_service_ is known.
  ManagementPolicy* management_policy_;

  // The id of the extension we are prompting the user about.
  std::string extension_id_prompting_;

  // If true, we will ignore notifications in ::Observe(). This is needed
  // to prevent reloading the page when we were the cause of the
  // notification.
  bool ignore_notifications_;

  // The page may be refreshed in response to a RenderViewHost being destroyed,
  // but the iteration over RenderViewHosts will include the host because the
  // notification is sent when it is in the process of being deleted (and before
  // it is removed from the process). Keep a pointer to it so we can exclude
  // it from the active views.
  content::RenderViewHost* deleting_rvh_;
  // Do the same for a deleting RenderWidgetHost ID and RenderProcessHost ID.
  int deleting_rwh_id_;
  int deleting_rph_id_;

  // We want to register for notifications only after we've responded at least
  // once to the page, otherwise we'd be calling JavaScript functions on objects
  // that don't exist yet when notifications come in. This variable makes sure
  // we do so only once.
  bool registered_for_notifications_;

  content::NotificationRegistrar registrar_;

  // The UI for showing what permissions the extension has.
  scoped_ptr<ExtensionInstallPrompt> prompt_;

  ScopedObserver<WarningService, WarningService::Observer>
      warning_service_observer_;

  // An observer to listen for when Extension errors are reported.
  ScopedObserver<ErrorConsole, ErrorConsole::Observer> error_console_observer_;

  // An observer to listen for notable changes in the ExtensionPrefs, like
  // a change in Disable Reasons.
  ScopedObserver<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observer_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  ScopedObserver<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observer_;

  // Whether we found any DISABLE_NOT_VERIFIED extensions and want to kick off
  // a verification check to try and rescue them.
  bool should_do_verification_check_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
