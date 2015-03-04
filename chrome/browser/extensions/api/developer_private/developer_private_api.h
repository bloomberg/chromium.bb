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
class RequirementsChecker;

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
  ~DeveloperPrivateEventRouter() override;

  // Add or remove an ID to the list of extensions subscribed to events.
  void AddExtensionId(const std::string& extension_id);
  void RemoveExtensionId(const std::string& extension_id);

 private:
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
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  bool from_ephemeral,
                                  const std::string& old_name) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // ErrorConsole::Observer implementation.
  void OnErrorAdded(const ExtensionError* error) override;

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
  ~DeveloperPrivateAPI() override;

  void SetLastUnpackedDirectory(const base::FilePath& path);

  base::FilePath& GetLastUnpackedDirectory() {
    return last_unpacked_directory_;
  }

  // KeyedService implementation
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

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

class DeveloperPrivateAPIFunction : public UIThreadExtensionFunction {
 protected:
  ~DeveloperPrivateAPIFunction() override;

  // Returns the extension with the given |id| from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const std::string& id);
};

class DeveloperPrivateAutoUpdateFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.autoUpdate",
                             DEVELOPERPRIVATE_AUTOUPDATE)

 protected:
  ~DeveloperPrivateAutoUpdateFunction() override;

  // ExtensionFunction:
  bool RunSync() override;
};

class DeveloperPrivateGetItemsInfoFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPERPRIVATE_GETITEMSINFO)

 protected:
  ~DeveloperPrivateGetItemsInfoFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

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
  ~DeveloperPrivateInspectFunction() override;

  // ExtensionFunction:
  bool RunSync() override;
};

class DeveloperPrivateAllowFileAccessFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.allowFileAccess",
                             DEVELOPERPRIVATE_ALLOWFILEACCESS);

 protected:
  ~DeveloperPrivateAllowFileAccessFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateAllowIncognitoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.allowIncognito",
                             DEVELOPERPRIVATE_ALLOWINCOGNITO);

 protected:
  ~DeveloperPrivateAllowIncognitoFunction() override;

  // UIThreadExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateReloadFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload",
                             DEVELOPERPRIVATE_RELOAD);

 protected:
  ~DeveloperPrivateReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateShowPermissionsDialogFunction
    : public ChromeSyncExtensionFunction,
      public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPermissionsDialog",
                             DEVELOPERPRIVATE_PERMISSIONS);

  DeveloperPrivateShowPermissionsDialogFunction();
 protected:
  ~DeveloperPrivateShowPermissionsDialogFunction() override;

  // ExtensionFunction:
  bool RunSync() override;

  // Overridden from ExtensionInstallPrompt::Delegate
  void InstallUIProceed() override;
  void InstallUIAbort(bool user_initiated) override;

  scoped_ptr<ExtensionInstallPrompt> prompt_;
  std::string extension_id_;
};

class DeveloperPrivateChooseEntryFunction : public UIThreadExtensionFunction,
                                            public EntryPickerClient {
 protected:
  ~DeveloperPrivateChooseEntryFunction() override;
  bool ShowPicker(ui::SelectFileDialog::Type picker_type,
                  const base::string16& select_title,
                  const ui::SelectFileDialog::FileTypeInfo& info,
                  int file_type_index);
};


class DeveloperPrivateLoadUnpackedFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadUnpacked",
                             DEVELOPERPRIVATE_LOADUNPACKED);
  DeveloperPrivateLoadUnpackedFunction();

 protected:
  ~DeveloperPrivateLoadUnpackedFunction() override;
  ResponseAction Run() override;

  // EntryPickerClient:
  void FileSelected(const base::FilePath& path) override;
  void FileSelectionCanceled() override;

  // Callback for the UnpackedLoader.
  void OnLoadComplete(const Extension* extension,
                      const base::FilePath& file_path,
                      const std::string& error);

 private:
  // Whether or not we should fail quietly in the event of a load error.
  bool fail_quietly_;
};

class DeveloperPrivateChoosePathFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.choosePath",
                             DEVELOPERPRIVATE_CHOOSEPATH);

 protected:
  ~DeveloperPrivateChoosePathFunction() override;
  ResponseAction Run() override;

  // EntryPickerClient:
  void FileSelected(const base::FilePath& path) override;
  void FileSelectionCanceled() override;
};

class DeveloperPrivatePackDirectoryFunction
    : public DeveloperPrivateAPIFunction,
      public PackExtensionJob::Client {

 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.packDirectory",
                             DEVELOPERPRIVATE_PACKDIRECTORY);

  DeveloperPrivatePackDirectoryFunction();

  // ExtensionPackJob::Client implementation.
  void OnPackSuccess(const base::FilePath& crx_file,
                     const base::FilePath& key_file) override;
  void OnPackFailure(const std::string& error,
                     ExtensionCreator::ErrorType error_type) override;

 protected:
  ~DeveloperPrivatePackDirectoryFunction() override;
  ResponseAction Run() override;

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
  ~DeveloperPrivateIsProfileManagedFunction() override;

  // ExtensionFunction:
  bool RunSync() override;
};

class DeveloperPrivateLoadDirectoryFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadDirectory",
                             DEVELOPERPRIVATE_LOADUNPACKEDCROS);

  DeveloperPrivateLoadDirectoryFunction();

 protected:
  ~DeveloperPrivateLoadDirectoryFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

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
  ~DeveloperPrivateRequestFileSourceFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

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
  ~DeveloperPrivateOpenDevToolsFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
