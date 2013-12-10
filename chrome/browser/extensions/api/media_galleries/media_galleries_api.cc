// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_galleries/media_galleries_api.h"

#include <set>
#include <string>
#include <vector>

#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/blob_reader.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/media_galleries/media_galleries_histograms.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/pref_names.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_sniffer.h"

using content::WebContents;
using web_modal::WebContentsModalDialogManager;

namespace extensions {

namespace MediaGalleries = api::media_galleries;
namespace GetMediaFileSystems = MediaGalleries::GetMediaFileSystems;

namespace {

const char kDisallowedByPolicy[] =
    "Media Galleries API is disallowed by policy: ";

const char kDeviceIdKey[] = "deviceId";
const char kGalleryIdKey[] = "galleryId";
const char kIsAvailableKey[] = "isAvailable";
const char kIsMediaDeviceKey[] = "isMediaDevice";
const char kIsRemovableKey[] = "isRemovable";
const char kNameKey[] = "name";

// Checks whether the MediaGalleries API is currently accessible (it may be
// disallowed even if an extension has the requisite permission).
bool ApiIsAccessible(std::string* error) {
  if (!ChromeSelectFilePolicy::FileSelectDialogsAllowed()) {
    *error = std::string(kDisallowedByPolicy) +
        prefs::kAllowFileSelectionDialogs;
    return false;
  }

  return true;
}

MediaFileSystemRegistry* media_file_system_registry() {
  return g_browser_process->media_file_system_registry();
}

}  // namespace

MediaGalleriesGetMediaFileSystemsFunction::
    ~MediaGalleriesGetMediaFileSystemsFunction() {}

bool MediaGalleriesGetMediaFileSystemsFunction::RunImpl() {
  if (!ApiIsAccessible(&error_))
    return false;

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

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::OnPreferencesInit,
      this,
      interactive));
  return true;
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
  content::RenderViewHost* rvh = render_view_host();
  if (!rvh) {
    SendResponse(false);
    return;
  }
  MediaGalleriesPermission::CheckParam read_param(
      MediaGalleriesPermission::kReadPermission);
  bool has_read_permission = PermissionsData::CheckAPIPermissionWithParam(
      GetExtension(), APIPermission::kMediaGalleries, &read_param);
  MediaGalleriesPermission::CheckParam copy_to_param(
      MediaGalleriesPermission::kCopyToPermission);
  bool has_copy_to_permission = PermissionsData::CheckAPIPermissionWithParam(
      GetExtension(), APIPermission::kMediaGalleries, &copy_to_param);
  MediaGalleriesPermission::CheckParam delete_param(
      MediaGalleriesPermission::kDeletePermission);
  bool has_delete_permission = PermissionsData::CheckAPIPermissionWithParam(
      GetExtension(), APIPermission::kMediaGalleries, &delete_param);

  const int child_id = rvh->GetProcess()->GetID();
  base::ListValue* list = new base::ListValue();
  for (size_t i = 0; i < filesystems.size(); i++) {
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
      policy->GrantReadFileSystem(child_id, filesystems[i].fsid);
      if (has_delete_permission) {
        policy->GrantDeleteFromFileSystem(child_id, filesystems[i].fsid);
        if (has_copy_to_permission) {
          policy->GrantCopyIntoFileSystem(child_id, filesystems[i].fsid);
        }
      }
    }
  }

  // The custom JS binding will use this list to create DOMFileSystem objects.
  SetResult(list);
  SendResponse(true);
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialog() {
  media_galleries::UsageCount(media_galleries::SHOW_DIALOG);
  WebContents* contents = WebContents::FromRenderViewHost(render_view_host());
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(contents);
  if (!web_contents_modal_dialog_manager) {
    // If there is no WebContentsModalDialogManager, then this contents is
    // probably the background page for an app. Try to find a shell window to
    // host the dialog.
    apps::ShellWindow* window = apps::ShellWindowRegistry::Get(
        GetProfile())->GetCurrentShellWindowForApp(GetExtension()->id());
    if (!window) {
      SendResponse(false);
      return;
    }
    contents = window->web_contents();
  }

  // Controller will delete itself.
  base::Closure cb = base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries, this);
  new MediaGalleriesDialogController(contents, *GetExtension(), cb);
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
  if (!ApiIsAccessible(&error_))
    return false;

  media_galleries::UsageCount(
      media_galleries::GET_ALL_MEDIA_FILE_SYSTEM_METADATA);
  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesGetAllMediaFileSystemMetadataFunction::OnPreferencesInit,
      this));
  return true;
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

MediaGalleriesGetMetadataFunction::~MediaGalleriesGetMetadataFunction() {}

bool MediaGalleriesGetMetadataFunction::RunImpl() {
  if (!ApiIsAccessible(&error_))
    return false;

  std::string blob_uuid;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &blob_uuid));

  const base::Value* options_value = NULL;
  if (!args_->Get(1, &options_value))
    return false;
  scoped_ptr<MediaGalleries::MediaMetadataOptions> options =
      MediaGalleries::MediaMetadataOptions::FromValue(*options_value);
  if (!options)
    return false;

  MediaGalleriesPreferences* preferences =
      media_file_system_registry()->GetPreferences(GetProfile());
  bool mime_type_only = options->metadata_type ==
      MediaGalleries::GET_METADATA_TYPE_MIMETYPEONLY;
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesGetMetadataFunction::OnPreferencesInit,
      this, mime_type_only, blob_uuid));

  return true;
}

void MediaGalleriesGetMetadataFunction::OnPreferencesInit(
    bool mime_type_only,
    const std::string& blob_uuid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // BlobReader is self-deleting.
  BlobReader* reader = new BlobReader(
      GetProfile(),
      blob_uuid,
      base::Bind(&MediaGalleriesGetMetadataFunction::SniffMimeType, this,
                 mime_type_only));
  reader->SetByteRange(0, net::kMaxBytesToSniff);
  reader->Start();
}

void MediaGalleriesGetMetadataFunction::SniffMimeType(
    bool mime_type_only, scoped_ptr<std::string> blob_header) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  MediaGalleries::MediaMetadata metadata;

  std::string mime_type;
  bool mime_type_sniffed = net::SniffMimeTypeFromLocalData(
      blob_header->c_str(), blob_header->size(), &mime_type);
  if (mime_type_sniffed)
    metadata.mime_type = mime_type;

  // TODO(tommycli): Kick off SafeMediaMetadataParser if |mime_type_only| false.

  SetResult(metadata.ToValue().release());
  SendResponse(true);
}

}  // namespace extensions
