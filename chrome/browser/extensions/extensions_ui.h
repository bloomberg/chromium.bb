// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_

#include <string>
#include <vector>

#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/browser/dom_ui/dom_ui.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class Extension;
class ExtensionsService;
class FilePath;
class PrefService;
class UserScript;
class Value;

// Information about a page running in an extension, for example a toolstrip,
// a background page, or a tab contents.
struct ExtensionPage {
  ExtensionPage(const GURL& url, int render_process_id, int render_view_id)
    : url(url), render_process_id(render_process_id),
      render_view_id(render_view_id) {}
  GURL url;
  int render_process_id;
  int render_view_id;
};

class ExtensionsUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  ExtensionsUIHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  ~ExtensionsUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(ExtensionsUIHTMLSource);
};

// The handler for Javascript messages related to the "extensions" view.
class ExtensionsDOMHandler
    : public DOMMessageHandler,
      public NotificationObserver,
      public PackExtensionJob::Client,
      public SelectFileDialog::Listener,
      public ExtensionInstallUI::Delegate {
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
    explicit IconLoader(ExtensionsDOMHandler* handler);

    // Load |icons|. Will call handler->OnIconsLoaded when complete. IconLoader
    // takes ownership of both arguments.
    void LoadIcons(std::vector<ExtensionResource>* icons,
                   DictionaryValue* json);

    // Cancel the load. IconLoader won't try to call back to the handler after
    // this.
    void Cancel();

   private:
    // Load the icons and call ReportResultOnUIThread when done. This method
    // takes ownership of both arguments.
    void LoadIconsOnFileThread(std::vector<ExtensionResource>* icons,
                               DictionaryValue* json);

    // Report back to the handler. This method takes ownership of |json|.
    void ReportResultOnUIThread(DictionaryValue* json);

    // The handler we will report back to.
    ExtensionsDOMHandler* handler_;
  };

  explicit ExtensionsDOMHandler(ExtensionsService* extension_service);
  virtual ~ExtensionsDOMHandler();

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

  // Extension Detail JSON Struct for page. (static for ease of testing).
  static DictionaryValue* CreateExtensionDetailValue(
      const Extension *extension,
      const std::vector<ExtensionPage>&,
      bool enabled);

  // ContentScript JSON Struct for page. (static for ease of testing).
  static DictionaryValue* CreateContentScriptDetailValue(
      const UserScript& script,
      const FilePath& extension_path);

  // ExtensionPackJob::Client
  virtual void OnPackSuccess(const FilePath& crx_file,
                             const FilePath& key_file);

  virtual void OnPackFailure(const std::wstring& message);

  // ExtensionInstallUI::Delegate implementation, used for receiving
  // notification about uninstall confirmation dialog selections.
  virtual void InstallUIProceed(bool create_app_shortcut);
  virtual void InstallUIAbort();

 private:
  // Callback for "requestExtensionsData" message.
  void HandleRequestExtensionsData(const Value* value);

  // Callback for "toggleDeveloperMode" message.
  void HandleToggleDeveloperMode(const Value* value);

  // Callback for "inspect" message.
  void HandleInspectMessage(const Value* value);

  // Callback for "reload" message.
  void HandleReloadMessage(const Value* value);

  // Callback for "enable" message.
  void HandleEnableMessage(const Value* value);

  // Callback for "uninstall" message.
  void HandleUninstallMessage(const Value* value);

  // Callback for "options" message.
  void HandleOptionsMessage(const Value* value);

  // Callback for "load" message.
  void HandleLoadMessage(const Value* value);

  // Callback for "pack" message.
  void HandlePackMessage(const Value* value);

  // Callback for "autoupdate" message.
  void HandleAutoUpdateMessage(const Value* value);

  // Utility for calling javascript window.alert in the page.
  void ShowAlert(const std::string& message);

  // Callback for "selectFilePath" message.
  void HandleSelectFilePathMessage(const Value* value);

  // SelectFileDialog::Listener
  virtual void FileSelected(const FilePath& path,
                            int index, void* params);
  virtual void MultiFilesSelected(
    const std::vector<FilePath>& files, void* params) {
    NOTREACHED();
  }
  virtual void FileSelectionCanceled(void* params) {}

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Helper that lists the current active html pages for an extension.
  std::vector<ExtensionPage> GetActivePagesForExtension(
      const std::string& extension_id);

  // Returns the best icon to display in the UI for an extension, or an empty
  // ExtensionResource if no good icon exists.
  ExtensionResource PickExtensionIcon(Extension* extension);

  // Loads the extension resources into the json data, then calls OnIconsLoaded.
  // Takes ownership of |icons|.
  // Called on the file thread.
  void LoadExtensionIcons(std::vector<ExtensionResource>* icons,
                          DictionaryValue* json_data);

  // Takes ownership of |json_data| and tells HTML about it.
  // Called on the UI thread.
  void OnIconsLoaded(DictionaryValue* json_data);

  // Our model.
  scoped_refptr<ExtensionsService> extensions_service_;

  // Used to pick the directory when loading an extension.
  scoped_refptr<SelectFileDialog> load_extension_dialog_;

  // Used to package the extension.
  scoped_refptr<PackExtensionJob> pack_job_;

  // Used to load icons asynchronously on the file thread.
  scoped_refptr<IconLoader> icon_loader_;

  // We monitor changes to the extension system so that we can reload when
  // necessary.
  NotificationRegistrar registrar_;

  // The id of the extension we are about to un-install.
  std::string extension_id_uninstalling_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsDOMHandler);
};

class ExtensionsUI : public DOMUI {
 public:
  explicit ExtensionsUI(TabContents* contents);

  static RefCountedMemory* GetFaviconResourceBytes();

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsUI);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSIONS_UI_H_
