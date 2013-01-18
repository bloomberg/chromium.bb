// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/requirements_checker.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_view_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"

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

}

}  // namespace api

}  // namespace extensions

namespace developer = extensions::api::developer_private;

typedef std::vector<linked_ptr<developer::ItemInfo> > ItemInfoList;
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

  void SetLastUnpackedDirectory(const FilePath& path);

  FilePath& getLastUnpackedDirectory() { return last_unpacked_directory_; }

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
  FilePath last_unpacked_directory_;

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

class DeveloperPrivateGetItemsInfoFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPERPRIVATE_GETITEMSINFO)

 protected:
  virtual ~DeveloperPrivateGetItemsInfoFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:

  void AddItemsInfo(const ExtensionSet& items,
                    ExtensionSystem* system,
                    ItemInfoList* item_list);

  scoped_ptr<developer::ItemInfo> CreateItemInfo(
      const extensions::Extension& item,
      ExtensionSystem* system,
      bool item_is_enabled);

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

class DeveloperPrivateReloadFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload",
                             DEVELOPERPRIVATE_RELOAD);

 protected:
  virtual ~DeveloperPrivateReloadFunction();

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

class DeveloperPrivateChooseEntryFunction : public SyncExtensionFunction,
                                            public EntryPickerClient {
 protected:
  virtual ~DeveloperPrivateChooseEntryFunction();
  virtual bool RunImpl() OVERRIDE;
  bool ShowPicker(ui::SelectFileDialog::Type picker_type,
                  const FilePath& last_directory,
                  const string16& select_title);

  // EntryPickerCLient functions.
  virtual void FileSelected(const FilePath& path) = 0;
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
  virtual void FileSelected(const FilePath& path) OVERRIDE;
  virtual void FileSelectionCanceled() OVERRIDE;

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

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
