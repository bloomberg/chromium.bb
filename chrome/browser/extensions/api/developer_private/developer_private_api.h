// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include <set>

#include "base/files/file.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry_observer.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace extensions {

class ExtensionError;
class ExtensionRegistry;
class ExtensionSystem;
class ManagementPolicy;

namespace api {

class EntryPicker;
class EntryPickerClient;

namespace developer_private {

struct ItemInfo;
struct ItemInspectView;
struct ProjectInfo;

}  // namespace developer_private

}  // namespace api

namespace developer = api::developer_private;

typedef std::vector<linked_ptr<developer::ItemInfo> > ItemInfoList;
typedef std::vector<linked_ptr<developer::ProjectInfo> > ProjectInfoList;
typedef std::vector<linked_ptr<developer::ItemInspectView> >
    ItemInspectViewList;

class DeveloperPrivateEventRouter : public content::NotificationObserver,
                                    public ExtensionRegistryObserver,
                                    public ErrorConsole::Observer {
 public:
  explicit DeveloperPrivateEventRouter(Profile* profile);
  virtual ~DeveloperPrivateEventRouter();

  // Add or remove an ID to the list of extensions subscribed to events.
  void AddExtensionId(const std::string& extension_id);
  void RemoveExtensionId(const std::string& extension_id);

 private:
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
  virtual void OnExtensionWillBeInstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      bool is_update,
      bool from_ephemeral,
      const std::string& old_name) OVERRIDE;
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // ErrorConsole::Observer implementation.
  virtual void OnErrorAdded(const ExtensionError* error) OVERRIDE;

  content::NotificationRegistrar registrar_;

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  Profile* profile_;

  // The set of IDs of the Extensions that have subscribed to DeveloperPrivate
  // events. Since the only consumer of the DeveloperPrivate API is currently
  // the Apps Developer Tool (which replaces the chrome://extensions page), we
  // don't want to send information about the subscribing extension in an
  // update. In particular, we want to avoid entering a loop, which could happen
  // when, e.g., the Apps Developer Tool throws an error.
  std::set<std::string> extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateEventRouter);
};

// The profile-keyed service that manages the DeveloperPrivate API.
class DeveloperPrivateAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer {
 public:
  static BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
      GetFactoryInstance();

  // Convenience method to get the DeveloperPrivateAPI for a profile.
  static DeveloperPrivateAPI* Get(content::BrowserContext* context);

  explicit DeveloperPrivateAPI(content::BrowserContext* context);
  virtual ~DeveloperPrivateAPI();

  void SetLastUnpackedDirectory(const base::FilePath& path);

  base::FilePath& GetLastUnpackedDirectory() {
    return last_unpacked_directory_;
  }

  // KeyedService implementation
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "DeveloperPrivateAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  void RegisterNotifications();

  Profile* profile_;

  // Used to start the load |load_extension_dialog_| in the last directory that
  // was loaded.
  base::FilePath last_unpacked_directory_;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<DeveloperPrivateEventRouter> developer_private_event_router_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateAPI);
};

namespace api {

class DeveloperPrivateAutoUpdateFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.autoUpdate",
                             DEVELOPERPRIVATE_AUTOUPDATE)

 protected:
  virtual ~DeveloperPrivateAutoUpdateFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class DeveloperPrivateGetItemsInfoFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPERPRIVATE_GETITEMSINFO)

 protected:
  virtual ~DeveloperPrivateGetItemsInfoFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  scoped_ptr<developer::ItemInfo> CreateItemInfo(const Extension& item,
                                                 bool item_is_enabled);

  void GetIconsOnFileThread(
      ItemInfoList item_list,
      std::map<std::string, ExtensionResource> itemIdToIconResourceMap);

  // Helper that lists the current inspectable html pages for the extension.
  void GetInspectablePagesForExtensionProcess(
      const Extension* extension,
      const std::set<content::RenderViewHost*>& views,
      ItemInspectViewList* result);

  ItemInspectViewList GetInspectablePagesForExtension(
      const Extension* extension,
      bool extension_is_enabled);

  void GetAppWindowPagesForExtensionProfile(const Extension* extension,
                                            ItemInspectViewList* result);

  linked_ptr<developer::ItemInspectView> constructInspectView(
      const GURL& url,
      int render_process_id,
      int render_view_id,
      bool incognito,
      bool generated_background_page);
};

class DeveloperPrivateInspectFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.inspect",
                             DEVELOPERPRIVATE_INSPECT)

 protected:
  virtual ~DeveloperPrivateInspectFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class DeveloperPrivateAllowFileAccessFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.allowFileAccess",
                             DEVELOPERPRIVATE_ALLOWFILEACCESS);

 protected:
  virtual ~DeveloperPrivateAllowFileAccessFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class DeveloperPrivateAllowIncognitoFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.allowIncognito",
                             DEVELOPERPRIVATE_ALLOWINCOGNITO);

 protected:
  virtual ~DeveloperPrivateAllowIncognitoFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class DeveloperPrivateReloadFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload",
                             DEVELOPERPRIVATE_RELOAD);

 protected:
  virtual ~DeveloperPrivateReloadFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class DeveloperPrivateShowPermissionsDialogFunction
    : public ChromeSyncExtensionFunction,
      public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPermissionsDialog",
                             DEVELOPERPRIVATE_PERMISSIONS);

  DeveloperPrivateShowPermissionsDialogFunction();
 protected:
  virtual ~DeveloperPrivateShowPermissionsDialogFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

  // Overridden from ExtensionInstallPrompt::Delegate
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  scoped_ptr<ExtensionInstallPrompt> prompt_;
  std::string extension_id_;
};

class DeveloperPrivateEnableFunction
    : public ChromeSyncExtensionFunction,
      public base::SupportsWeakPtr<DeveloperPrivateEnableFunction> {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.enable",
                             DEVELOPERPRIVATE_ENABLE);

  DeveloperPrivateEnableFunction();

 protected:
  virtual ~DeveloperPrivateEnableFunction();

  // Callback for requirements checker.
  void OnRequirementsChecked(const std::string& extension_id,
                             std::vector<std::string> requirements_errors);
  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;

 private:
  scoped_ptr<RequirementsChecker> requirements_checker_;
};

class DeveloperPrivateChooseEntryFunction : public ChromeAsyncExtensionFunction,
                                            public EntryPickerClient {
 protected:
  virtual ~DeveloperPrivateChooseEntryFunction();
  virtual bool RunAsync() OVERRIDE;
  bool ShowPicker(ui::SelectFileDialog::Type picker_type,
                  const base::FilePath& last_directory,
                  const base::string16& select_title,
                  const ui::SelectFileDialog::FileTypeInfo& info,
                  int file_type_index);

  // EntryPickerClient functions.
  virtual void FileSelected(const base::FilePath& path) = 0;
  virtual void FileSelectionCanceled() = 0;
};


class DeveloperPrivateLoadUnpackedFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadUnpacked",
                             DEVELOPERPRIVATE_LOADUNPACKED);

 protected:
  virtual ~DeveloperPrivateLoadUnpackedFunction();
  virtual bool RunAsync() OVERRIDE;

  // EntryPickerCLient implementation.
  virtual void FileSelected(const base::FilePath& path) OVERRIDE;
  virtual void FileSelectionCanceled() OVERRIDE;
};

class DeveloperPrivateChoosePathFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.choosePath",
                             DEVELOPERPRIVATE_CHOOSEPATH);

 protected:
  virtual ~DeveloperPrivateChoosePathFunction();
  virtual bool RunAsync() OVERRIDE;

  // EntryPickerClient functions.
  virtual void FileSelected(const base::FilePath& path) OVERRIDE;
  virtual void FileSelectionCanceled() OVERRIDE;
};

class DeveloperPrivatePackDirectoryFunction
    : public ChromeAsyncExtensionFunction,
      public PackExtensionJob::Client {

 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.packDirectory",
                             DEVELOPERPRIVATE_PACKDIRECTORY);

  DeveloperPrivatePackDirectoryFunction();

  // ExtensionPackJob::Client implementation.
  virtual void OnPackSuccess(const base::FilePath& crx_file,
                             const base::FilePath& key_file) OVERRIDE;
  virtual void OnPackFailure(const std::string& error,
                             ExtensionCreator::ErrorType error_type) OVERRIDE;

 protected:
  virtual ~DeveloperPrivatePackDirectoryFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  scoped_refptr<PackExtensionJob> pack_job_;
  std::string item_path_str_;
  std::string key_path_str_;
};

class DeveloperPrivateIsProfileManagedFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.isProfileManaged",
                             DEVELOPERPRIVATE_ISPROFILEMANAGED);

 protected:
  virtual ~DeveloperPrivateIsProfileManagedFunction();

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class DeveloperPrivateLoadDirectoryFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadDirectory",
                             DEVELOPERPRIVATE_LOADUNPACKEDCROS);

  DeveloperPrivateLoadDirectoryFunction();

 protected:
  virtual ~DeveloperPrivateLoadDirectoryFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  bool LoadByFileSystemAPI(const storage::FileSystemURL& directory_url);

  void ClearExistingDirectoryContent(const base::FilePath& project_path);

  void ReadDirectoryByFileSystemAPI(const base::FilePath& project_path,
                                    const base::FilePath& destination_path);

  void ReadDirectoryByFileSystemAPICb(
      const base::FilePath& project_path,
      const base::FilePath& destination_path,
      base::File::Error result,
      const storage::FileSystemOperation::FileEntryList& file_list,
      bool has_more);

  void SnapshotFileCallback(
      const base::FilePath& target_path,
      base::File::Error result,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<storage::ShareableFileReference>& file_ref);

  void CopyFile(const base::FilePath& src_path,
                const base::FilePath& dest_path);

  void Load();

  scoped_refptr<storage::FileSystemContext> context_;

  // syncfs url representing the root of the folder to be copied.
  std::string project_base_url_;

  // physical path on disc of the folder to be copied.
  base::FilePath project_base_path_;

 private:
  int pending_copy_operations_count_;

  // This is set to false if any of the copyFile operations fail on
  // call of the API. It is returned as a response of the API call.
  bool success_;
};

class DeveloperPrivateRequestFileSourceFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.requestFileSource",
                             DEVELOPERPRIVATE_REQUESTFILESOURCE);

  DeveloperPrivateRequestFileSourceFunction();

 protected:
  virtual ~DeveloperPrivateRequestFileSourceFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  void LaunchCallback(const base::DictionaryValue& results);
};

class DeveloperPrivateOpenDevToolsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.openDevTools",
                             DEVELOPERPRIVATE_OPENDEVTOOLS);

  DeveloperPrivateOpenDevToolsFunction();

 protected:
  virtual ~DeveloperPrivateOpenDevToolsFunction();

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
