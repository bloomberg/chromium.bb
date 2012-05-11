// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"

class Extension;
class ExtensionHost;
class ExtensionService;
class FilePath;
class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
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
class ExtensionSettingsHandler : public content::WebUIMessageHandler,
                                 public content::NotificationObserver,
                                 public content::WebContentsObserver,
                                 public SelectFileDialog::Listener,
                                 public ExtensionUninstallDialog::Delegate {
 public:
  ExtensionSettingsHandler();
  virtual ~ExtensionSettingsHandler();

  static void RegisterUserPrefs(PrefService* prefs);

  // Extension Detail JSON Struct for page. |pages| is injected for unit
  // testing.
  // Note: |warning_set| can be NULL in unit tests.
  base::DictionaryValue* CreateExtensionDetailValue(
      const Extension* extension,
      const std::vector<ExtensionPage>& pages,
      const ExtensionWarningSet* warning_set);

  void GetLocalizedValues(base::DictionaryValue* localized_strings);

  // content::WebContentsObserver implementation, which reloads all unpacked
  // extensions whenever chrome://extensions is reloaded.
  virtual void NavigateToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) OVERRIDE;

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE {}

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ExtensionUninstallDialog::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

  // Helper method that reloads all unpacked extensions.
  void ReloadUnpackedExtensions();

  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const base::ListValue* args);

  // Callback for "toggleDeveloperMode" message.
  void HandleToggleDeveloperMode(const base::ListValue* args);

  // Callback for "inspect" message.
  void HandleInspectMessage(const base::ListValue* args);

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
  const Extension* GetActiveExtension(const base::ListValue* args);

  // Forces a UI update if appropriate after a notification is received.
  void MaybeUpdateAfterNotification();

  // Register for notifications that we need to reload the page.
  void MaybeRegisterForNotifications();

  // Helper that lists the current inspectable html pages for an extension.
  std::vector<ExtensionPage> GetInspectablePagesForExtension(
      const Extension* extension, bool extension_is_enabled);
  void GetInspectablePagesForExtensionProcess(
      const std::set<content::RenderViewHost*>& views,
      std::vector<ExtensionPage> *result);

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Helper to inspect an ExtensionHost after it has been loaded.
  void InspectExtensionHost(ExtensionHost* host);

  // Our model.  Outlives us since it's owned by our containing profile.
  // Note: This may be NULL in unit tests.
  ExtensionService* extension_service_;

  // Used to pick the directory when loading an extension.
  scoped_refptr<SelectFileDialog> load_extension_dialog_;

  // Used to start the |load_extension_dialog_| in the last directory that was
  // loaded.
  FilePath last_unpacked_directory_;

  // Used to show confirmation UI for uninstalling extensions in incognito mode.
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

  // The id of the extension we are prompting the user about.
  std::string extension_id_prompting_;

  // If true, we will ignore notifications in ::Observe(). This is needed
  // to prevent reloading the page when we were the cause of the
  // notification.
  bool ignore_notifications_;

  // The page may be refreshed in response to a RENDER_VIEW_HOST_DELETED,
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

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_HANDLER_H_
