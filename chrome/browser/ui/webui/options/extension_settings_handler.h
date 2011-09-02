// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_EXTENSION_SETTINGS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_EXTENSION_SETTINGS_HANDLER_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/common/extensions/extension_resource.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class Extension;
class ExtensionService;
class FilePath;
class PrefService;
class RenderProcessHost;
class UserScript;

namespace base {
class DictionaryValue;
class ListValue;
}

// Information about a page running in an extension, for example a toolstrip,
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
class ExtensionSettingsHandler : public OptionsPageUIHandler,
                                 public SelectFileDialog::Listener,
                                 public ExtensionUninstallDialog::Delegate {
 public:
  // Helper class that loads the icons for the extensions in the management UI.
  // We do this with native code instead of just using chrome-extension:// URLs
  // for two reasons:
  //
  // 1. We need to support the disabled extensions, too, and using URLs won't
  //    work for them.
  // 2. We want to desaturate the icons of the disabled extensions to make them
  //    look disabled.
  class IconLoader : public base::RefCountedThreadSafe<IconLoader> {
   public:
    explicit IconLoader(ExtensionSettingsHandler* handler);

    // Load |icons|. Will call handler->OnIconsLoaded when complete. IconLoader
    // takes ownership of both arguments.
    void LoadIcons(std::vector<ExtensionResource>* icons,
                   base::DictionaryValue* json);

    // Cancel the load. IconLoader won't try to call back to the handler after
    // this.
    void Cancel();

   private:
    // Load the icons and call ReportResultOnUIThread when done. This method
    // takes ownership of both arguments.
    void LoadIconsOnFileThread(std::vector<ExtensionResource>* icons,
                               base::DictionaryValue* json);

    // Report back to the handler. This method takes ownership of |json|.
    void ReportResultOnUIThread(base::DictionaryValue* json);

    // The handler we will report back to.
    ExtensionSettingsHandler* handler_;
  };

  ExtensionSettingsHandler();
  virtual ~ExtensionSettingsHandler();

  // Extension Detail JSON Struct for page. (static for ease of testing).
  // Note: service can be NULL in unit tests.
  static base::DictionaryValue* CreateExtensionDetailValue(
      ExtensionService* service,
      const Extension* extension,
      const std::vector<ExtensionPage>& pages,
      bool enabled,
      bool terminated);

  // ContentScript JSON Struct for page. (static for ease of testing).
  static base::DictionaryValue* CreateContentScriptDetailValue(
      const UserScript& script,
      const FilePath& extension_path);

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

  // Callback for "load" message.
  void HandleLoadMessage(const base::ListValue* args);

  // Callback for "pack" message.
  void HandlePackMessage(const base::ListValue* args);

  // Callback for "autoupdate" message.
  void HandleAutoUpdateMessage(const base::ListValue* args);

  // Utility for calling javascript window.alert in the page.
  void ShowAlert(const std::string& message);

  // Callback for "selectFilePath" message.
  void HandleSelectFilePathMessage(const base::ListValue* args);

  // Utility for callbacks that get an extension ID as the sole argument.
  const Extension* GetExtension(const base::ListValue* args);

  // Forces a UI update if appropriate after a notification is received.
  void MaybeUpdateAfterNotification();

  // SelectFileDialog::Listener
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // ExtensionUninstallDialog::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void ExtensionDialogAccepted() OVERRIDE;
  virtual void ExtensionDialogCanceled() OVERRIDE;

 private:
  // Helper that lists the current active html pages for an extension.
  std::vector<ExtensionPage> GetActivePagesForExtension(
      const Extension* extension);
  void GetActivePagesForExtensionProcess(
      RenderProcessHost* process,
      const Extension* extension,
      std::vector<ExtensionPage> *result);

  // Returns the best icon to display in the UI for an extension, or an empty
  // ExtensionResource if no good icon exists.
  ExtensionResource PickExtensionIcon(const Extension* extension);

  // Loads the extension resources into the json data, then calls OnIconsLoaded.
  // Takes ownership of |icons|.
  // Called on the file thread.
  void LoadExtensionIcons(std::vector<ExtensionResource>* icons,
                          base::DictionaryValue* json_data);

  // Takes ownership of |json_data| and tells HTML about it.
  // Called on the UI thread.
  void OnIconsLoaded(base::DictionaryValue* json_data);

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Our model.  Outlives us since it's owned by our containing profile.
  ExtensionService* extension_service_;

  // Used to pick the directory when loading an extension.
  scoped_refptr<SelectFileDialog> load_extension_dialog_;

  // Used to load icons asynchronously on the file thread.
  scoped_refptr<IconLoader> icon_loader_;

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
  RenderViewHost* deleting_rvh_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_EXTENSION_SETTINGS_HANDLER_H_
