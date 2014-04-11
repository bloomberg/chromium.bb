// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_galleries/media_galleries_api.h"

#include <set>
#include <string>
#include <vector>

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/blob_reader.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/media_galleries/fileapi/safe_media_metadata_parser.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/media_galleries/media_galleries_scan_result_dialog_controller.h"
#include "chrome/browser/media_galleries/media_scan_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/pref_names.h"
#include "components/storage_monitor/storage_info.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/media_galleries_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "net/base/mime_sniffer.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using storage_monitor::MediaStorageUtil;
using storage_monitor::StorageInfo;
using web_modal::WebContentsModalDialogManager;

namespace extensions {

namespace MediaGalleries = api::media_galleries;
namespace DropPermissionForMediaFileSystem =
    MediaGalleries::DropPermissionForMediaFileSystem;
namespace GetMediaFileSystems = MediaGalleries::GetMediaFileSystems;

namespace {

const char kDisallowedByPolicy[] =
    "Media Galleries API is disallowed by policy: ";
const char kFailedToSetGalleryPermission[] =
    "Failed to set gallery permission.";
const char kInvalidGalleryId[] = "Invalid gallery id.";
const char kMissingEventListener[] = "Missing event listener registration.";
const char kNonExistentGalleryId[] = "Non-existent gallery id.";
const char kNoScanPermission[] = "No permission to scan.";

const char kDeviceIdKey[] = "deviceId";
const char kGalleryIdKey[] = "galleryId";
const char kIsAvailableKey[] = "isAvailable";
const char kIsMediaDeviceKey[] = "isMediaDevice";
const char kIsRemovableKey[] = "isRemovable";
const char kNameKey[] = "name";

MediaFileSystemRegistry* media_file_system_registry() {
  return g_browser_process->media_file_system_registry();
}

MediaScanManager* media_scan_manager() {
  return media_file_system_registry()->media_scan_manager();
}

// Checks whether the MediaGalleries API is currently accessible (it may be
// disallowed even if an extension has the requisite permission). Then
// initializes the MediaGalleriesPreferences
bool Setup(Profile* profile, std::string* error, base::Closure callback) {
  if (!ChromeSelectFilePolicy::FileSelectDialogsAllowed()) {
    *error = std::string(kDisallowedByPolicy) +
        prefs::kAllowFileSelectionDialogs;
    return false;
  }

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(profile);
  preferences->EnsureInitialized(callback);
  return true;
}

WebContents* GetWebContents(content::RenderViewHost* rvh,
                            Profile* profile,
                            const std::string& app_id) {
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(contents);
  if (!web_contents_modal_dialog_manager) {
    // If there is no WebContentsModalDialogManager, then this contents is
    // probably the background page for an app. Try to find a app window to
    // host the dialog.
    apps::AppWindow* window = apps::AppWindowRegistry::Get(profile)
                                  ->GetCurrentAppWindowForApp(app_id);
    contents = window ? window->web_contents() : NULL;
  }
  return contents;
}

base::ListValue* ConstructFileSystemList(
    content::RenderViewHost* rvh,
    const Extension* extension,
    const std::vector<MediaFileSystemInfo>& filesystems) {
  if (!rvh)
    return NULL;

  MediaGalleriesPermission::CheckParam read_param(
      MediaGalleriesPermission::kReadPermission);
  bool has_read_permission = PermissionsData::CheckAPIPermissionWithParam(
      extension, APIPermission::kMediaGalleries, &read_param);
  MediaGalleriesPermission::CheckParam copy_to_param(
      MediaGalleriesPermission::kCopyToPermission);
  bool has_copy_to_permission = PermissionsData::CheckAPIPermissionWithParam(
      extension, APIPermission::kMediaGalleries, &copy_to_param);
  MediaGalleriesPermission::CheckParam delete_param(
      MediaGalleriesPermission::kDeletePermission);
  bool has_delete_permission = PermissionsData::CheckAPIPermissionWithParam(
      extension, APIPermission::kMediaGalleries, &delete_param);

  const int child_id = rvh->GetProcess()->GetID();
  scoped_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < filesystems.size(); ++i) {
    scoped_ptr<base::DictionaryValue> file_system_dict_value(
        new base::DictionaryValue());

    // Send the file system id so the renderer can create a valid FileSystem
    // object.
    file_system_dict_value->SetStringWithoutPathExpansion(
        "fsid", filesystems[i].fsid);

    file_system_dict_value->SetStringWithoutPathExpansion(
        kNameKey, filesystems[i].name);
    file_system_dict_value->SetStringWithoutPathExpansion(
        kGalleryIdKey,
        base::Uint64ToString(filesystems[i].pref_id));
    if (!filesystems[i].transient_device_id.empty()) {
      file_system_dict_value->SetStringWithoutPathExpansion(
          kDeviceIdKey, filesystems[i].transient_device_id);
    }
    file_system_dict_value->SetBooleanWithoutPathExpansion(
        kIsRemovableKey, filesystems[i].removable);
    file_system_dict_value->SetBooleanWithoutPathExpansion(
        kIsMediaDeviceKey, filesystems[i].media_device);
    file_system_dict_value->SetBooleanWithoutPathExpansion(
        kIsAvailableKey, true);

    list->Append(file_system_dict_value.release());

    if (filesystems[i].path.empty())
      continue;

    if (has_read_permission) {
      content::ChildProcessSecurityPolicy* policy =
          content::ChildProcessSecurityPolicy::GetInstance();
      policy->GrantReadFile(child_id, filesystems[i].path);
      if (has_delete_permission) {
        policy->GrantDeleteFrom(child_id, filesystems[i].path);
        if (has_copy_to_permission) {
          policy->GrantCopyInto(child_id, filesystems[i].path);
        }
      }
    }
  }

  return list.release();
}

bool CheckScanPermission(const extensions::Extension* extension,
                         std::string* error) {
  DCHECK(extension);
  DCHECK(error);
  MediaGalleriesPermission::CheckParam scan_param(
      MediaGalleriesPermission::kScanPermission);
  bool has_scan_permission = PermissionsData::CheckAPIPermissionWithParam(
      extension, APIPermission::kMediaGalleries, &scan_param);
  if (!has_scan_permission)
    *error = kNoScanPermission;
  return has_scan_permission;
}

class SelectDirectoryDialog : public ui::SelectFileDialog::Listener,
                              public base::RefCounted<SelectDirectoryDialog> {
 public:
  // Selected file path, or an empty path if the user canceled.
  typedef base::Callback<void(const base::FilePath&)> Callback;

  SelectDirectoryDialog(WebContents* web_contents, const Callback& callback)
      : web_contents_(web_contents),
        callback_(callback) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents));
  }

  void Show(const base::FilePath& default_path) {
    AddRef();  // Balanced in the two reachable listener outcomes.
    select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_MEDIA_GALLERIES_DIALOG_ADD_GALLERY_TITLE),
      default_path,
      NULL,
      0,
      base::FilePath::StringType(),
      platform_util::GetTopLevel(web_contents_->GetView()->GetNativeView()),
      NULL);
  }

  // ui::SelectFileDialog::Listener implementation.
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE {
    callback_.Run(path);
    Release();  // Balanced in Show().
  }

  virtual void MultiFilesSelected(const std::vector<base::FilePath>& files,
                                  void* params) OVERRIDE {
    NOTREACHED() << "Should not be able to select multiple files";
  }

  virtual void FileSelectionCanceled(void* params) OVERRIDE {
    callback_.Run(base::FilePath());
    Release();  // Balanced in Show().
  }

 private:
  friend class base::RefCounted<SelectDirectoryDialog>;
  virtual ~SelectDirectoryDialog() {}

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  WebContents* web_contents_;
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(SelectDirectoryDialog);
};

}  // namespace

MediaGalleriesEventRouter::MediaGalleriesEventRouter(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)), weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  media_scan_manager()->AddObserver(profile_, this);
}

MediaGalleriesEventRouter::~MediaGalleriesEventRouter() {
}

void MediaGalleriesEventRouter::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_ptr_factory_.InvalidateWeakPtrs();
  media_scan_manager()->RemoveObserver(profile_);
  media_scan_manager()->CancelScansForProfile(profile_);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MediaGalleriesEventRouter> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MediaGalleriesEventRouter>*
MediaGalleriesEventRouter::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
MediaGalleriesEventRouter* MediaGalleriesEventRouter::Get(
    content::BrowserContext* context) {
  DCHECK(media_file_system_registry()
             ->GetPreferences(Profile::FromBrowserContext(context))
             ->IsInitialized());
  return BrowserContextKeyedAPIFactory<MediaGalleriesEventRouter>::Get(context);
}

bool MediaGalleriesEventRouter::ExtensionHasScanProgressListener(
    const std::string& extension_id) const {
  EventRouter* router = ExtensionSystem::Get(profile_)->event_router();
  return router->ExtensionHasEventListener(
      extension_id,
      MediaGalleries::OnScanProgress::kEventName);
}

void MediaGalleriesEventRouter::OnScanStarted(const std::string& extension_id) {
  MediaGalleries::ScanProgressDetails details;
  details.type = MediaGalleries::SCAN_PROGRESS_TYPE_START;
  DispatchEventToExtension(
      extension_id,
      MediaGalleries::OnScanProgress::kEventName,
      MediaGalleries::OnScanProgress::Create(details).Pass());
}

void MediaGalleriesEventRouter::OnScanCancelled(
    const std::string& extension_id) {
  MediaGalleries::ScanProgressDetails details;
  details.type = MediaGalleries::SCAN_PROGRESS_TYPE_CANCEL;
  DispatchEventToExtension(
      extension_id,
      MediaGalleries::OnScanProgress::kEventName,
      MediaGalleries::OnScanProgress::Create(details).Pass());
}

void MediaGalleriesEventRouter::OnScanFinished(
    const std::string& extension_id, int gallery_count,
    const MediaGalleryScanResult& file_counts) {
  media_galleries::UsageCount(media_galleries::SCAN_FINISHED);
  MediaGalleries::ScanProgressDetails details;
  details.type = MediaGalleries::SCAN_PROGRESS_TYPE_FINISH;
  details.gallery_count.reset(new int(gallery_count));
  details.audio_count.reset(new int(file_counts.audio_count));
  details.image_count.reset(new int(file_counts.image_count));
  details.video_count.reset(new int(file_counts.video_count));
  DispatchEventToExtension(
      extension_id,
      MediaGalleries::OnScanProgress::kEventName,
      MediaGalleries::OnScanProgress::Create(details).Pass());
}

void MediaGalleriesEventRouter::OnScanError(
    const std::string& extension_id) {
  MediaGalleries::ScanProgressDetails details;
  details.type = MediaGalleries::SCAN_PROGRESS_TYPE_ERROR;
  DispatchEventToExtension(
      extension_id,
      MediaGalleries::OnScanProgress::kEventName,
      MediaGalleries::OnScanProgress::Create(details).Pass());
}

void MediaGalleriesEventRouter::DispatchEventToExtension(
    const std::string& extension_id,
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  EventRouter* router =
      extensions::ExtensionSystem::Get(profile_)->event_router();
  if (!router->ExtensionHasEventListener(extension_id, event_name))
    return;

  scoped_ptr<extensions::Event> event(
      new extensions::Event(event_name, event_args.Pass()));
  router->DispatchEventToExtension(extension_id, event.Pass());
}

MediaGalleriesGetMediaFileSystemsFunction::
    ~MediaGalleriesGetMediaFileSystemsFunction() {}

bool MediaGalleriesGetMediaFileSystemsFunction::RunImpl() {
  media_galleries::UsageCount(media_galleries::GET_MEDIA_FILE_SYSTEMS);
  scoped_ptr<GetMediaFileSystems::Params> params(
      GetMediaFileSystems::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  MediaGalleries::GetMediaFileSystemsInteractivity interactive =
      MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NO;
  if (params->details.get() && params->details->interactive != MediaGalleries::
         GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NONE) {
    interactive = params->details->interactive;
  }

  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit, this,
      interactive));
}

void MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit(
    MediaGalleries::GetMediaFileSystemsInteractivity interactive) {
  switch (interactive) {
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_YES: {
      // The MediaFileSystemRegistry only updates preferences for extensions
      // that it knows are in use. Since this may be the first call to
      // chrome.getMediaFileSystems for this extension, call
      // GetMediaFileSystemsForExtension() here solely so that
      // MediaFileSystemRegistry will send preference changes.
      GetMediaFileSystemsForExtension(base::Bind(
          &MediaGalleriesGetMediaFileSystemsFunction::AlwaysShowDialog, this));
      return;
    }
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_IF_NEEDED: {
      GetMediaFileSystemsForExtension(base::Bind(
          &MediaGalleriesGetMediaFileSystemsFunction::ShowDialogIfNoGalleries,
          this));
      return;
    }
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NO:
      GetAndReturnGalleries();
      return;
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NONE:
      NOTREACHED();
  }
  SendResponse(false);
}

void MediaGalleriesGetMediaFileSystemsFunction::AlwaysShowDialog(
    const std::vector<MediaFileSystemInfo>& /*filesystems*/) {
  ShowDialog();
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialogIfNoGalleries(
    const std::vector<MediaFileSystemInfo>& filesystems) {
  if (filesystems.empty())
    ShowDialog();
  else
    ReturnGalleries(filesystems);
}

void MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries() {
  GetMediaFileSystemsForExtension(base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries, this));
}

void MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries(
    const std::vector<MediaFileSystemInfo>& filesystems) {
  scoped_ptr<base::ListValue> list(
      ConstructFileSystemList(render_view_host(), GetExtension(), filesystems));
  if (!list.get()) {
    SendResponse(false);
    return;
  }

  // The custom JS binding will use this list to create DOMFileSystem objects.
  SetResult(list.release());
  SendResponse(true);
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialog() {
  media_galleries::UsageCount(media_galleries::SHOW_DIALOG);
  const Extension* extension = GetExtension();
  WebContents* contents =
      GetWebContents(render_view_host(), GetProfile(), extension->id());
  if (!contents) {
    SendResponse(false);
    return;
  }

  // Controller will delete itself.
  base::Closure cb = base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries, this);
  new MediaGalleriesDialogController(contents, *extension, cb);
}

void MediaGalleriesGetMediaFileSystemsFunction::GetMediaFileSystemsForExtension(
    const MediaFileSystemsCallback& cb) {
  if (!render_view_host()) {
    cb.Run(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  DCHECK(registry->GetPreferences(GetProfile())->IsInitialized());
  registry->GetMediaFileSystemsForExtension(
      render_view_host(), GetExtension(), cb);
}

MediaGalleriesGetAllMediaFileSystemMetadataFunction::
    ~MediaGalleriesGetAllMediaFileSystemMetadataFunction() {}

bool MediaGalleriesGetAllMediaFileSystemMetadataFunction::RunImpl() {
  media_galleries::UsageCount(
      media_galleries::GET_ALL_MEDIA_FILE_SYSTEM_METADATA);
  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesGetAllMediaFileSystemMetadataFunction::OnPreferencesInit,
      this));
}

void MediaGalleriesGetAllMediaFileSystemMetadataFunction::OnPreferencesInit() {
  MediaFileSystemRegistry* registry = media_file_system_registry();
  MediaGalleriesPreferences* prefs = registry->GetPreferences(GetProfile());
  DCHECK(prefs->IsInitialized());
  MediaGalleryPrefIdSet permitted_gallery_ids =
      prefs->GalleriesForExtension(*GetExtension());

  MediaStorageUtil::DeviceIdSet* device_ids = new MediaStorageUtil::DeviceIdSet;
  const MediaGalleriesPrefInfoMap& galleries = prefs->known_galleries();
  for (MediaGalleryPrefIdSet::const_iterator it = permitted_gallery_ids.begin();
       it != permitted_gallery_ids.end(); ++it) {
    MediaGalleriesPrefInfoMap::const_iterator gallery_it = galleries.find(*it);
    DCHECK(gallery_it != galleries.end());
    device_ids->insert(gallery_it->second.device_id);
  }

  MediaStorageUtil::FilterAttachedDevices(
      device_ids,
      base::Bind(
          &MediaGalleriesGetAllMediaFileSystemMetadataFunction::OnGetGalleries,
          this,
          permitted_gallery_ids,
          base::Owned(device_ids)));
}

void MediaGalleriesGetAllMediaFileSystemMetadataFunction::OnGetGalleries(
    const MediaGalleryPrefIdSet& permitted_gallery_ids,
    const MediaStorageUtil::DeviceIdSet* available_devices) {
  MediaFileSystemRegistry* registry = media_file_system_registry();
  MediaGalleriesPreferences* prefs = registry->GetPreferences(GetProfile());

  base::ListValue* list = new base::ListValue();
  const MediaGalleriesPrefInfoMap& galleries = prefs->known_galleries();
  for (MediaGalleryPrefIdSet::const_iterator it = permitted_gallery_ids.begin();
       it != permitted_gallery_ids.end(); ++it) {
    MediaGalleriesPrefInfoMap::const_iterator gallery_it = galleries.find(*it);
    DCHECK(gallery_it != galleries.end());
    const MediaGalleryPrefInfo& gallery = gallery_it->second;
    MediaGalleries::MediaFileSystemMetadata metadata;
    metadata.name = base::UTF16ToUTF8(gallery.GetGalleryDisplayName());
    metadata.gallery_id = base::Uint64ToString(gallery.pref_id);
    metadata.is_removable = StorageInfo::IsRemovableDevice(gallery.device_id);
    metadata.is_media_device = StorageInfo::IsMediaDevice(gallery.device_id);
    metadata.is_available = ContainsKey(*available_devices, gallery.device_id);
    list->Append(metadata.ToValue().release());
  }

  SetResult(list);
  SendResponse(true);
}

MediaGalleriesAddUserSelectedFolderFunction::
    ~MediaGalleriesAddUserSelectedFolderFunction() {}

bool MediaGalleriesAddUserSelectedFolderFunction::RunImpl() {
  media_galleries::UsageCount(media_galleries::ADD_USER_SELECTED_FOLDER);
  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesAddUserSelectedFolderFunction::OnPreferencesInit, this));
}

void MediaGalleriesAddUserSelectedFolderFunction::OnPreferencesInit() {
  Profile* profile = GetProfile();
  const std::string& app_id = GetExtension()->id();
  WebContents* contents = GetWebContents(render_view_host(), profile, app_id);
  if (!contents) {
    // When the request originated from a background page, but there is no app
    // window open, check to see if it originated from a tab and display the
    // dialog in that tab.
    bool found_tab = extensions::ExtensionTabUtil::GetTabById(
        source_tab_id(), profile, profile->IsOffTheRecord(),
        NULL, NULL, &contents, NULL);
    if (!found_tab || !contents) {
      SendResponse(false);
      return;
    }
  }

  if (!user_gesture()) {
    OnDirectorySelected(base::FilePath());
    return;
  }

  base::FilePath last_used_path =
      extensions::file_system_api::GetLastChooseEntryDirectory(
          extensions::ExtensionPrefs::Get(profile), app_id);
  SelectDirectoryDialog::Callback callback = base::Bind(
      &MediaGalleriesAddUserSelectedFolderFunction::OnDirectorySelected, this);
  scoped_refptr<SelectDirectoryDialog> select_directory_dialog =
      new SelectDirectoryDialog(contents, callback);
  select_directory_dialog->Show(last_used_path);
}

void MediaGalleriesAddUserSelectedFolderFunction::OnDirectorySelected(
    const base::FilePath& selected_directory) {
  if (selected_directory.empty()) {
    // User cancelled case.
    GetMediaFileSystemsForExtension(base::Bind(
        &MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId,
        this,
        kInvalidMediaGalleryPrefId));
    return;
  }

  extensions::file_system_api::SetLastChooseEntryDirectory(
      extensions::ExtensionPrefs::Get(GetProfile()),
      GetExtension()->id(),
      selected_directory);

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  MediaGalleryPrefId pref_id =
      preferences->AddGalleryByPath(selected_directory,
                                    MediaGalleryPrefInfo::kUserAdded);
  preferences->SetGalleryPermissionForExtension(*GetExtension(), pref_id, true);

  GetMediaFileSystemsForExtension(base::Bind(
      &MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId,
      this,
      pref_id));
}

void MediaGalleriesAddUserSelectedFolderFunction::ReturnGalleriesAndId(
    MediaGalleryPrefId pref_id,
    const std::vector<MediaFileSystemInfo>& filesystems) {
  scoped_ptr<base::ListValue> list(
      ConstructFileSystemList(render_view_host(), GetExtension(), filesystems));
  if (!list.get()) {
    SendResponse(false);
    return;
  }

  int index = -1;
  if (pref_id != kInvalidMediaGalleryPrefId) {
    for (size_t i = 0; i < filesystems.size(); ++i) {
      if (filesystems[i].pref_id == pref_id) {
        index = i;
        break;
      }
    }
  }
  base::DictionaryValue* results = new base::DictionaryValue;
  results->SetWithoutPathExpansion("mediaFileSystems", list.release());
  results->SetIntegerWithoutPathExpansion("selectedFileSystemIndex", index);
  SetResult(results);
  SendResponse(true);
}

void
MediaGalleriesAddUserSelectedFolderFunction::GetMediaFileSystemsForExtension(
    const MediaFileSystemsCallback& cb) {
  if (!render_view_host()) {
    cb.Run(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  DCHECK(registry->GetPreferences(GetProfile())->IsInitialized());
  registry->GetMediaFileSystemsForExtension(
      render_view_host(), GetExtension(), cb);
}

MediaGalleriesDropPermissionForMediaFileSystemFunction::
    ~MediaGalleriesDropPermissionForMediaFileSystemFunction() {}

bool MediaGalleriesDropPermissionForMediaFileSystemFunction::RunImpl() {
  media_galleries::UsageCount(
      media_galleries::DROP_PERMISSION_FOR_MEDIA_FILE_SYSTEM);

  scoped_ptr<DropPermissionForMediaFileSystem::Params> params(
      DropPermissionForMediaFileSystem::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  MediaGalleryPrefId pref_id;
  if (!base::StringToUint64(params->gallery_id, &pref_id)) {
    error_ = kInvalidGalleryId;
    return false;
  }

  base::Closure callback = base::Bind(
      &MediaGalleriesDropPermissionForMediaFileSystemFunction::
          OnPreferencesInit,
      this,
      pref_id);
  return Setup(GetProfile(), &error_, callback);
}

void MediaGalleriesDropPermissionForMediaFileSystemFunction::OnPreferencesInit(
    MediaGalleryPrefId pref_id) {
  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  if (!ContainsKey(preferences->known_galleries(), pref_id)) {
    error_ = kNonExistentGalleryId;
    SendResponse(false);
    return;
  }

  bool dropped = preferences->SetGalleryPermissionForExtension(
      *GetExtension(), pref_id, false);
  if (dropped)
    SetResult(new base::StringValue(base::Uint64ToString(pref_id)));
  else
    error_ = kFailedToSetGalleryPermission;
  SendResponse(dropped);
}

MediaGalleriesStartMediaScanFunction::~MediaGalleriesStartMediaScanFunction() {}

bool MediaGalleriesStartMediaScanFunction::RunImpl() {
  media_galleries::UsageCount(media_galleries::START_MEDIA_SCAN);
  if (!CheckScanPermission(GetExtension(), &error_)) {
    MediaGalleriesEventRouter::Get(GetProfile())->OnScanError(
        GetExtension()->id());
    return false;
  }
  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesStartMediaScanFunction::OnPreferencesInit, this));
}

void MediaGalleriesStartMediaScanFunction::OnPreferencesInit() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MediaGalleriesEventRouter* api = MediaGalleriesEventRouter::Get(GetProfile());
  if (!api->ExtensionHasScanProgressListener(GetExtension()->id())) {
    error_ = kMissingEventListener;
    SendResponse(false);
    return;
  }

  media_scan_manager()->StartScan(GetProfile(), GetExtension(), user_gesture());
  SendResponse(true);
}

MediaGalleriesCancelMediaScanFunction::
    ~MediaGalleriesCancelMediaScanFunction() {
}

bool MediaGalleriesCancelMediaScanFunction::RunImpl() {
  media_galleries::UsageCount(media_galleries::CANCEL_MEDIA_SCAN);
  if (!CheckScanPermission(GetExtension(), &error_)) {
    MediaGalleriesEventRouter::Get(GetProfile())->OnScanError(
        GetExtension()->id());
    return false;
  }
  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesCancelMediaScanFunction::OnPreferencesInit, this));
}

void MediaGalleriesCancelMediaScanFunction::OnPreferencesInit() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  media_scan_manager()->CancelScan(GetProfile(), GetExtension());
  SendResponse(true);
}

MediaGalleriesAddScanResultsFunction::~MediaGalleriesAddScanResultsFunction() {}

bool MediaGalleriesAddScanResultsFunction::RunImpl() {
  media_galleries::UsageCount(media_galleries::ADD_SCAN_RESULTS);
  if (!CheckScanPermission(GetExtension(), &error_)) {
    // We don't fire a scan progress error here, as it would be unintuitive.
    return false;
  }
  if (!user_gesture())
    return false;

  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesAddScanResultsFunction::OnPreferencesInit, this));
}

MediaGalleriesScanResultDialogController*
MediaGalleriesAddScanResultsFunction::MakeDialog(
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    const base::Closure& on_finish) {
  // Controller will delete itself.
  return new MediaGalleriesScanResultDialogController(web_contents, extension,
                                                      on_finish);
}

void MediaGalleriesAddScanResultsFunction::OnPreferencesInit() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const Extension* extension = GetExtension();
  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  if (MediaGalleriesScanResultDialogController::ScanResultCountForExtension(
          preferences, extension) == 0) {
    GetAndReturnGalleries();
    return;
  }

  WebContents* contents =
      GetWebContents(render_view_host(), GetProfile(), extension->id());
  if (!contents) {
    SendResponse(false);
    return;
  }

  base::Closure cb = base::Bind(
      &MediaGalleriesAddScanResultsFunction::GetAndReturnGalleries, this);
  MakeDialog(contents, *extension, cb);
}

void MediaGalleriesAddScanResultsFunction::GetAndReturnGalleries() {
  if (!render_view_host()) {
    ReturnGalleries(std::vector<MediaFileSystemInfo>());
    return;
  }
  MediaFileSystemRegistry* registry = media_file_system_registry();
  DCHECK(registry->GetPreferences(GetProfile())->IsInitialized());
  registry->GetMediaFileSystemsForExtension(
      render_view_host(), GetExtension(),
      base::Bind(&MediaGalleriesAddScanResultsFunction::ReturnGalleries,
                 this));
}

void MediaGalleriesAddScanResultsFunction::ReturnGalleries(
    const std::vector<MediaFileSystemInfo>& filesystems) {
  scoped_ptr<base::ListValue> list(
      ConstructFileSystemList(render_view_host(), GetExtension(), filesystems));
  if (!list.get()) {
    SendResponse(false);
    return;
  }

  // The custom JS binding will use this list to create DOMFileSystem objects.
  SetResult(list.release());
  SendResponse(true);
}

MediaGalleriesGetMetadataFunction::~MediaGalleriesGetMetadataFunction() {}

bool MediaGalleriesGetMetadataFunction::RunImpl() {
  std::string blob_uuid;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &blob_uuid));

  const base::Value* options_value = NULL;
  if (!args_->Get(1, &options_value))
    return false;
  scoped_ptr<MediaGalleries::MediaMetadataOptions> options =
      MediaGalleries::MediaMetadataOptions::FromValue(*options_value);
  if (!options)
    return false;

  bool mime_type_only = options->metadata_type ==
      MediaGalleries::GET_METADATA_TYPE_MIMETYPEONLY;

  return Setup(GetProfile(), &error_, base::Bind(
      &MediaGalleriesGetMetadataFunction::OnPreferencesInit, this,
      mime_type_only, blob_uuid));
}

void MediaGalleriesGetMetadataFunction::OnPreferencesInit(
    bool mime_type_only, const std::string& blob_uuid) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // BlobReader is self-deleting.
  BlobReader* reader = new BlobReader(
      GetProfile(),
      blob_uuid,
      base::Bind(&MediaGalleriesGetMetadataFunction::SniffMimeType, this,
                 mime_type_only, blob_uuid));
  reader->SetByteRange(0, net::kMaxBytesToSniff);
  reader->Start();
}

void MediaGalleriesGetMetadataFunction::SniffMimeType(
    bool mime_type_only, const std::string& blob_uuid,
    scoped_ptr<std::string> blob_header, int64 total_blob_length) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string mime_type;
  bool mime_type_sniffed = net::SniffMimeTypeFromLocalData(
      blob_header->c_str(), blob_header->size(), &mime_type);

  if (!mime_type_sniffed) {
    SendResponse(false);
    return;
  }

  if (mime_type_only) {
    MediaGalleries::MediaMetadata metadata;
    metadata.mime_type = mime_type;
    SetResult(metadata.ToValue().release());
    SendResponse(true);
    return;
  }

  scoped_refptr<metadata::SafeMediaMetadataParser> parser(
      new metadata::SafeMediaMetadataParser(GetProfile(), blob_uuid,
                                            total_blob_length, mime_type));
  parser->Start(base::Bind(
      &MediaGalleriesGetMetadataFunction::OnSafeMediaMetadataParserDone, this));
}

void MediaGalleriesGetMetadataFunction::OnSafeMediaMetadataParserDone(
    bool parse_success, base::DictionaryValue* metadata_dictionary) {
  if (!parse_success) {
    SendResponse(false);
    return;
  }

  SetResult(metadata_dictionary->DeepCopy());
  SendResponse(true);
}

}  // namespace extensions
