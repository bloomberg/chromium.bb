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
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_warning_service.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class ExtensionService;
class PrefRegistrySyncable;

namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
}

namespace content {
class WebUIDataSource;
}

namespace extensions {
class Extension;
class ExtensionHost;
class ManagementPolicy;
}

// Information about a page running in an extension, for example a popup bubble,
// a background page, or a tab contents.
struct ExtensionPage {
  ExtensionPage(const GURL& url, int render_process_id, int render_view_id,
                bool incognito)
      : url(url),
        render_process_id(render_process_id),
        render_view_id(render_view_id),
        incognito(incognito) {}
  GURL url;
  int render_process_id;
  int render_view_id;
  bool incognito;
};

// Extension Settings UI handler.
class ExtensionSettingsHandler
    : public content::WebUIMessageHandler,
      public content::NotificationObserver,
      public content::WebContentsObserver,
      public ui::SelectFileDialog::Listener,
      public ExtensionInstallPrompt::Delegate,
      public ExtensionUninstallDialog::Delegate,
      public extensions::ExtensionWarningService::Observer,
      public base::SupportsWeakPtr<ExtensionSettingsHandler> {
 public:
  ExtensionSettingsHandler();
  virtual ~ExtensionSettingsHandler();

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Extension Detail JSON Struct for page. |pages| is injected for unit
  // testing.
  // Note: |warning_service| can be NULL in unit tests.
  base::DictionaryValue* CreateExtensionDetailValue(
      const extensions::Extension* extension,
      const std::vector<ExtensionPage>& pages,
      const extensions::ExtensionWarningService* warning_service);

  void GetLocalizedValues(content::WebUIDataSource* source);

  // content::WebContentsObserver implementation.
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;

 private:
  friend class ExtensionUITest;

  // Allows injection for testing by friend classes.
  ExtensionSettingsHandler(ExtensionService* service,
                           extensions::ManagementPolicy* policy);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(
      const std::vector<base::FilePath>& files, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE {}

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionUninstallDialog::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // extensions::ExtensionWarningService::Observer implementation.
  virtual void ExtensionWarningsChanged() OVERRIDE;

  // ExtensionInstallPrompt::Delegate implementation.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // Helper method that reloads all unpacked extensions.
  void ReloadUnpackedExtensions();

  // Callback for "setElevated" message.
  void ManagedUserSetElevated(const base::ListValue* args);

  // If the authentication of the managed user was successful,
  // it gives this information back to the UI.
  void PassphraseDialogCallback(bool success);

  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const base::ListValue* args);

  // Callback for "toggleDeveloperMode" message.
  void HandleToggleDeveloperMode(const base::ListValue* args);

  // Callback for "inspect" message.
  void HandleInspectMessage(const base::ListValue* args);

  // Callback for "launch" message.
  void HandleLaunchMessage(const ListValue* args);

  // Callback for "restart" message.
  void HandleRestartMessage(const ListValue* args);

  // Callback for "reload" message.
  void HandleReloadMessage(const base::ListValue* args);

  // Callback for "enable" message.
  void HandleEnableMessage(const base::ListValue* args);

  // Callback for "enableIncognito" message.
  void HandleEnableIncognitoMessage(const base::ListValue* args);

  // Callback for "allowFileAcces" message.
  void HandleAllowFileAccessMessage(const base::ListValue* args);

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

  // Callback for "loadUnpackedExtension" message.
  void HandleLoadUnpackedExtensionMessage(const base::ListValue* args);

  // Utility for calling JavaScript window.alert in the page.
  void ShowAlert(const std::string& message);

  // Utility for callbacks that get an extension ID as the sole argument.
  // Returns NULL if the extension isn't active.
  const extensions::Extension* GetActiveExtension(const base::ListValue* args);

  // Forces a UI update if appropriate after a notification is received.
  void MaybeUpdateAfterNotification();

  // Register for notifications that we need to reload the page.
  void MaybeRegisterForNotifications();

  // Helper that lists the current inspectable html pages for an extension.
  std::vector<ExtensionPage> GetInspectablePagesForExtension(
      const extensions::Extension* extension, bool extension_is_enabled);
  void GetInspectablePagesForExtensionProcess(
      const std::set<content::RenderViewHost*>& views,
      std::vector<ExtensionPage>* result);
  void GetShellWindowPagesForExtensionProfile(
      const extensions::Extension* extension,
      Profile* profile,
      std::vector<ExtensionPage>* result);

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Callback for RequirementsChecker.
  void OnRequirementsChecked(std::string extension_id,
                             std::vector<std::string> requirement_errors);

  // Our model.  Outlives us since it's owned by our containing profile.
  ExtensionService* extension_service_;

  // A convenience member, filled once the extension_service_ is known.
  extensions::ManagementPolicy* management_policy_;

  // Used to pick the directory when loading an extension.
  scoped_refptr<ui::SelectFileDialog> load_extension_dialog_;

  // Used to start the |load_extension_dialog_| in the last directory that was
  // loaded.
  base::FilePath last_unpacked_directory_;

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
  scoped_ptr<extensions::RequirementsChecker> requirements_checker_;

  // The UI for showing what permissions the extension has.
  scoped_ptr<ExtensionInstallPrompt> prompt_;

  ScopedObserver<extensions::ExtensionWarningService,
                 extensions::ExtensionWarningService::Observer>
      warning_service_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
