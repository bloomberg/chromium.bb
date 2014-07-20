// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_warning_service.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"
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
      public ExtensionPrefsObserver,
      public ExtensionRegistryObserver,
      public ExtensionUninstallDialog::Delegate,
      public ExtensionWarningService::Observer,
      public base::SupportsWeakPtr<ExtensionSettingsHandler> {
 public:
  ExtensionSettingsHandler();
  virtual ~ExtensionSettingsHandler();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Extension Detail JSON Struct for page. |pages| is injected for unit
  // testing.
  // Note: |warning_service| can be NULL in unit tests.
  base::DictionaryValue* CreateExtensionDetailValue(
      const Extension* extension,
      const std::vector<ExtensionPage>& pages,
      const ExtensionWarningService* warning_service);

  void GetLocalizedValues(content::WebUIDataSource* source);

 private:
  friend class ExtensionUITest;
  friend class BrokerDelegate;

  // content::WebContentsObserver implementation.
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;

  // Allows injection for testing by friend classes.
  ExtensionSettingsHandler(ExtensionService* service,
                           ManagementPolicy* policy);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // ErrorConsole::Observer implementation.
  virtual void OnErrorAdded(const ExtensionError* error) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // ExtensionPrefsObserver implementation.
  virtual void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                                int disable_reasons) OVERRIDE;

  // ExtensionUninstallDialog::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // ExtensionWarningService::Observer implementation.
  virtual void ExtensionWarningsChanged() OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Helper method that reloads all unpacked extensions.
  void ReloadUnpackedExtensions();

  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const base::ListValue* args);

  // Callback for "toggleDeveloperMode" message.
  void HandleToggleDeveloperMode(const base::ListValue* args);

  // Callback for "inspect" message.
  void HandleInspectMessage(const base::ListValue* args);

  // Callback for "launch" message.
  void HandleLaunchMessage(const base::ListValue* args);

  // Callback for "reload" message.
  void HandleReloadMessage(const base::ListValue* args);

  // Callback for "enable" message.
  void HandleEnableMessage(const base::ListValue* args);

  // Callback for "enableIncognito" message.
  void HandleEnableIncognitoMessage(const base::ListValue* args);

  // Callback for "enableErrorCollection" message.
  void HandleEnableErrorCollectionMessage(const base::ListValue* args);

  // Callback for "allowFileAcces" message.
  void HandleAllowFileAccessMessage(const base::ListValue* args);

  // Callback for "allowOnAllUrls" message.
  void HandleAllowOnAllUrlsMessage(const base::ListValue* args);

  // Callback for "uninstall" message.
  void HandleUninstallMessage(const base::ListValue* args);

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

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Callback for RequirementsChecker.
  void OnRequirementsChecked(std::string extension_id,
                             std::vector<std::string> requirement_errors);

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

  // Used to show confirmation UI for uninstalling extensions in incognito mode.
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

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

  PrefChangeRegistrar pref_registrar_;

  // This will not be empty when a requirements check is in progress. Doing
  // another Check() before the previous one is complete will cause the first
  // one to abort.
  scoped_ptr<RequirementsChecker> requirements_checker_;

  // The UI for showing what permissions the extension has.
  scoped_ptr<ExtensionInstallPrompt> prompt_;

  ScopedObserver<ExtensionWarningService, ExtensionWarningService::Observer>
      warning_service_observer_;

  // An observer to listen for when Extension errors are reported.
  ScopedObserver<ErrorConsole, ErrorConsole::Observer> error_console_observer_;

  // An observer to listen for notable changes in the ExtensionPrefs, like
  // a change in Disable Reasons.
  ScopedObserver<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observer_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  // Whether we found any DISABLE_NOT_VERIFIED extensions and want to kick off
  // a verification check to try and rescue them.
  bool should_do_verification_check_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
