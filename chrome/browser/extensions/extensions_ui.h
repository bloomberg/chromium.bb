// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
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
    : url(url), render_process_id(render_process_id),
      render_view_id(render_view_id), incognito(incognito) {}
  GURL url;
  int render_process_id;
  int render_view_id;
  bool incognito;
};

// The handler for JavaScript messages related to the "extensions" view.
class ExtensionsDOMHandler : public WebUIMessageHandler,
                             public NotificationObserver,
                             public PackExtensionJob::Client,
                             public SelectFileDialog::Listener,
                             public ExtensionUninstallDialog::Delegate {
 public:
  explicit ExtensionsDOMHandler(ExtensionService* extension_service);
  virtual ~ExtensionsDOMHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Extension Detail JSON Struct for page. (static for ease of testing).
  // Note: service can be NULL in unit tests.
  static base::DictionaryValue* CreateExtensionDetailValue(
      ExtensionService* service,
      const Extension* extension,
      const std::vector<ExtensionPage>& pages,
      bool enabled,
      bool terminated);

  // ExtensionPackJob::Client
  virtual void OnPackSuccess(const FilePath& crx_file,
                             const FilePath& key_file) OVERRIDE;

  virtual void OnPackFailure(const std::string& error) OVERRIDE;

  // ExtensionUninstallDialog::Delegate:
  virtual void ExtensionUninstallAccepted() OVERRIDE;
  virtual void ExtensionUninstallCanceled() OVERRIDE;

 private:
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

  // Register for notifications that we need to reload the page.
  void RegisterForNotifications();

  // SelectFileDialog::Listener
  virtual void FileSelected(const FilePath& path,
                            int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(
      const std::vector<FilePath>& files, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE {}

  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Helper that lists the current active html pages for an extension.
  std::vector<ExtensionPage> GetActivePagesForExtension(
      const Extension* extension);
  void GetActivePagesForExtensionProcess(
      RenderProcessHost* process,
      const Extension* extension,
      std::vector<ExtensionPage> *result);

  // Returns the ExtensionUninstallDialog object for this class, creating it if
  // needed.
  ExtensionUninstallDialog* GetExtensionUninstallDialog();

  // Our model.  Outlives us since it's owned by our containing profile.
  ExtensionService* const extension_service_;

  // Used to pick the directory when loading an extension.
  scoped_refptr<SelectFileDialog> load_extension_dialog_;

  // Used to package the extension.
  scoped_refptr<PackExtensionJob> pack_job_;

  // Used to show confirmation UI for uninstalling extensions in incognito mode.
  scoped_ptr<ExtensionUninstallDialog> extension_uninstall_dialog_;

  // The id of the extension we are prompting the user about.
  std::string extension_id_prompting_;

  // We monitor changes to the extension system so that we can reload when
  // necessary.
  NotificationRegistrar registrar_;

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

  DISALLOW_COPY_AND_ASSIGN(ExtensionsDOMHandler);
};

class ExtensionsUI : public ChromeWebUI {
 public:
  explicit ExtensionsUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsUI);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
