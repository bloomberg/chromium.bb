// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include "base/platform_file.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"

class ExtensionService;

namespace extensions {

class ExtensionSystem;
class ManagementPolicy;

namespace api {

class EntryPicker;
class EntryPickerClient;

namespace developer_private {

struct ItemInfo;
struct ItemInspectView;
struct ProjectInfo;

}

}  // namespace api

}  // namespace extensions

namespace developer = extensions::api::developer_private;

typedef std::vector<linked_ptr<developer::ItemInfo> > ItemInfoList;
typedef std::vector<linked_ptr<developer::ProjectInfo> > ProjectInfoList;
typedef std::vector<linked_ptr<developer::ItemInspectView> >
    ItemInspectViewList;

namespace extensions {

// The profile-keyed service that manages the DeveloperPrivate API.
class DeveloperPrivateAPI : public ProfileKeyedService,
                            public content::NotificationObserver {
 public:
  // Convenience method to get the DeveloperPrivateAPI for a profile.
  static DeveloperPrivateAPI* Get(Profile* profile);

  explicit DeveloperPrivateAPI(Profile* profile);
  virtual ~DeveloperPrivateAPI();

  void SetLastUnpackedDirectory(const base::FilePath& path);

  base::FilePath& GetLastUnpackedDirectory() {
    return last_unpacked_directory_;
  }

  // ProfileKeyedService implementation
  virtual void Shutdown() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void RegisterNotifications();

  // Used to start the load |load_extension_dialog_| in the last directory that
  // was loaded.
  base::FilePath last_unpacked_directory_;

  content::NotificationRegistrar registrar_;

};

namespace api {

class DeveloperPrivateAutoUpdateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.autoUpdate",
                             DEVELOPERPRIVATE_AUTOUPDATE)

 protected:
  virtual ~DeveloperPrivateAutoUpdateFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateGetItemsInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPERPRIVATE_GETITEMSINFO)

 protected:
  virtual ~DeveloperPrivateGetItemsInfoFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:

  scoped_ptr<developer::ItemInfo> CreateItemInfo(
      const extensions::Extension& item,
      bool item_is_enabled);

  void GetIconsOnFileThread(
      ItemInfoList item_list,
      std::map<std::string, ExtensionResource> itemIdToIconResourceMap);

  // Helper that lists the current inspectable html pages for the extension.
  void GetInspectablePagesForExtensionProcess(
      const std::set<content::RenderViewHost*>& views,
      ItemInspectViewList* result);

  ItemInspectViewList GetInspectablePagesForExtension(
      const extensions::Extension* extension,
      bool extension_is_enabled);

  void GetShellWindowPagesForExtensionProfile(
      const extensions::Extension* extension,
      ItemInspectViewList* result);

  linked_ptr<developer::ItemInspectView> constructInspectView(
      const GURL& url,
      int render_process_id,
      int render_view_id,
      bool incognito);
};

class DeveloperPrivateInspectFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.inspect",
                             DEVELOPERPRIVATE_INSPECT)

 protected:
  virtual ~DeveloperPrivateInspectFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateAllowFileAccessFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.allowFileAccess",
                             DEVELOPERPRIVATE_ALLOWFILEACCESS);

 protected:
  virtual ~DeveloperPrivateAllowFileAccessFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateAllowIncognitoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.allowIncognito",
                             DEVELOPERPRIVATE_ALLOWINCOGNITO);

 protected:
  virtual ~DeveloperPrivateAllowIncognitoFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateReloadFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload",
                             DEVELOPERPRIVATE_RELOAD);

 protected:
  virtual ~DeveloperPrivateReloadFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateShowPermissionsDialogFunction
    : public SyncExtensionFunction,
      public ExtensionInstallPrompt::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPermissionsDialog",
                             DEVELOPERPRIVATE_PERMISSIONS);

  DeveloperPrivateShowPermissionsDialogFunction();
 protected:
  virtual ~DeveloperPrivateShowPermissionsDialogFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Overridden from ExtensionInstallPrompt::Delegate
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  scoped_ptr<ExtensionInstallPrompt> prompt_;

};

class DeveloperPrivateRestartFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.restart",
                             DEVELOPERPRIVATE_RESTART);

 protected:
  virtual ~DeveloperPrivateRestartFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateEnableFunction
    : public SyncExtensionFunction,
      public base::SupportsWeakPtr<DeveloperPrivateEnableFunction> {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.enable",
                             DEVELOPERPRIVATE_ENABLE);

  DeveloperPrivateEnableFunction();

 protected:
  virtual ~DeveloperPrivateEnableFunction();

  // Callback for requirements checker.
  void OnRequirementsChecked(std::string extension_id,
                             std::vector<std::string> requirements_errors);
  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  scoped_ptr<extensions::RequirementsChecker> requirements_checker_;
};

class DeveloperPrivateChooseEntryFunction : public AsyncExtensionFunction,
                                            public EntryPickerClient {
 protected:
  virtual ~DeveloperPrivateChooseEntryFunction();
  virtual bool RunImpl() OVERRIDE;
  bool ShowPicker(ui::SelectFileDialog::Type picker_type,
                  const base::FilePath& last_directory,
                  const string16& select_title,
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
  virtual bool RunImpl() OVERRIDE;

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
  virtual bool RunImpl() OVERRIDE;

  // EntryPickerClient functions.
  virtual void FileSelected(const base::FilePath& path) OVERRIDE;
  virtual void FileSelectionCanceled() OVERRIDE;
};

class DeveloperPrivatePackDirectoryFunction
    : public AsyncExtensionFunction,
      public extensions::PackExtensionJob::Client {

 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.packDirectory",
                             DEVELOPERPRIVATE_PACKDIRECTORY);

  DeveloperPrivatePackDirectoryFunction();

  // ExtensionPackJob::Client implementation.
  virtual void OnPackSuccess(const base::FilePath& crx_file,
                             const base::FilePath& key_file) OVERRIDE;
  virtual void OnPackFailure(
      const std::string& error,
      extensions::ExtensionCreator::ErrorType error_type) OVERRIDE;

 protected:
  virtual ~DeveloperPrivatePackDirectoryFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  scoped_refptr<extensions::PackExtensionJob> pack_job_;
  std::string item_path_str_;
  std::string key_path_str_;
};

class DeveloperPrivateGetStringsFunction : public SyncExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("developerPrivate.getStrings",
                              DEVELOPERPRIVATE_GETSTRINGS);

  protected:
   virtual ~DeveloperPrivateGetStringsFunction();

   // ExtensionFunction
   virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateExportSyncfsFolderToLocalfsFunction
    : public AsyncExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("developerPrivate.exportSyncfsFolderToLocalfs",
                              DEVELOPERPRIVATE_LOADUNPACKEDCROS);

   DeveloperPrivateExportSyncfsFolderToLocalfsFunction();

  protected:
   virtual ~DeveloperPrivateExportSyncfsFolderToLocalfsFunction();

   // ExtensionFunction
   virtual bool RunImpl() OVERRIDE;

   void ClearPrexistingDirectoryContent(const base::FilePath& project_path);

   void ReadSyncFileSystemDirectory(const base::FilePath& project_path);

   void ReadSyncFileSystemDirectoryCb(
       const base::FilePath& project_path,
       base::PlatformFileError result,
       const fileapi::FileSystemOperation::FileEntryList& file_list,
       bool has_more);

   void CreateFolderAndSendResponse(const base::FilePath& project_path);

   void SnapshotFileCallback(
       const base::FilePath& target_path,
       base::PlatformFileError result,
       const base::PlatformFileInfo& file_info,
       const base::FilePath& platform_path,
       const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

   void CopyFile(const base::FilePath& src_path,
                 const base::FilePath& dest_path);

   scoped_refptr<fileapi::FileSystemContext> context_;

  private:
   int pendingCallbacksCount_;

   // This is set to false if any of the copyFile operations fail on
   // call of the API. It is returned as a response of the API call.
   bool success_;
};

class DeveloperPrivateLoadProjectToSyncfsFunction
    : public AsyncExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("developerPrivate.loadProjectToSyncfs",
                              DEVELOPERPRIVATE_LOADPROJECTTOSYNCFS);

   DeveloperPrivateLoadProjectToSyncfsFunction();

  protected:
   virtual ~DeveloperPrivateLoadProjectToSyncfsFunction();

   // ExtensionFunction
   virtual bool RunImpl() OVERRIDE;

   void CopyFolder(const base::FilePath::StringType& project_name);

   void CopyFiles(const std::vector<base::FilePath>& paths);

   void CopyFilesCallback(const base::PlatformFileError result);

  private:
   // Number of pending copy files callbacks.
   // It should only be modified on the IO Thread.
   int pendingCallbacksCount_;

   // True only when all the copyFiles job are successful.
   // It should only be modified on the IO thread.
   bool success_;

   scoped_refptr<fileapi::FileSystemContext> context_;
};

class DeveloperPrivateGetProjectsInfoFunction : public AsyncExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("developerPrivate.getProjectsInfo",
                              DEVELOPERPRIVATE_GETPROJECTSINFO);

   DeveloperPrivateGetProjectsInfoFunction();

  protected:
   virtual ~DeveloperPrivateGetProjectsInfoFunction();

   void ReadFolder();

   // ExtensionFunction
   virtual bool RunImpl() OVERRIDE;
};

class DeveloperPrivateLoadProjectFunction : public SyncExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("developerPrivate.loadProject",
                              DEVELOPERPRIVATE_LOADPROJECT);

   DeveloperPrivateLoadProjectFunction();

  protected:
   virtual ~DeveloperPrivateLoadProjectFunction();

   // ExtensionFunction
   virtual bool RunImpl() OVERRIDE;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
