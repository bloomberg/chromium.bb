// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_galleries/media_galleries_api.h"

#include <set>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/api/experimental_media_galleries.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_WIN)
#include "base/strings/sys_string_conversions.h"
#endif

using chrome::MediaFileSystemInfo;
using chrome::MediaFileSystemRegistry;
using chrome::MediaFileSystemsCallback;
using content::ChildProcessSecurityPolicy;
using content::WebContents;

namespace MediaGalleries = extensions::api::media_galleries;
namespace GetMediaFileSystems = MediaGalleries::GetMediaFileSystems;

namespace extensions {

namespace {

const char kDisallowedByPolicy[] =
    "Media Galleries API is disallowed by policy: ";
const char kInvalidInteractive[] = "Unknown value for interactive.";

const char kIsRemovableKey[] = "isRemovable";
const char kIsMediaDeviceKey[] = "isMediaDevice";

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

}  // namespace

MediaGalleriesGetMediaFileSystemsFunction::
    ~MediaGalleriesGetMediaFileSystemsFunction() {}

bool MediaGalleriesGetMediaFileSystemsFunction::RunImpl() {
  if (!ApiIsAccessible(&error_))
    return false;

  scoped_ptr<GetMediaFileSystems::Params> params(
      GetMediaFileSystems::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  MediaGalleries::GetMediaFileSystemsInteractivity interactive =
      MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NO;
  if (params->details.get() && params->details->interactive != MediaGalleries::
         GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NONE) {
    interactive = params->details->interactive;
  }

  switch (interactive) {
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_YES: {
      // The MediaFileSystemRegistry only updates preferences for extensions
      // that it knows are in use. Since this may be the first call to
      // chrome.getMediaFileSystems for this extension, call
      // GetMediaFileSystemsForExtension() here solely so that
      // MediaFileSystemRegistry will send preference changes.
      GetMediaFileSystemsForExtension(base::Bind(
          &MediaGalleriesGetMediaFileSystemsFunction::AlwaysShowDialog, this));
      return true;
    }
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_IF_NEEDED: {
      GetMediaFileSystemsForExtension(base::Bind(
          &MediaGalleriesGetMediaFileSystemsFunction::ShowDialogIfNoGalleries,
          this));
      return true;
    }
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NO:
      GetAndReturnGalleries();
      return true;
    case MediaGalleries::GET_MEDIA_FILE_SYSTEMS_INTERACTIVITY_NONE:
      NOTREACHED();
  }
  error_ = kInvalidInteractive;
  return false;
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
  bool has_read_permission = GetExtension()->CheckAPIPermissionWithParam(
      APIPermission::kMediaGalleries, &read_param);

  const int child_id = rvh->GetProcess()->GetID();
  std::set<std::string> file_system_names;
  base::ListValue* list = new base::ListValue();
  for (size_t i = 0; i < filesystems.size(); i++) {
    scoped_ptr<base::DictionaryValue> file_system_dict_value(
        new base::DictionaryValue());

    // Send the file system id so the renderer can create a valid FileSystem
    // object.
    file_system_dict_value->SetStringWithoutPathExpansion(
        "fsid", filesystems[i].fsid);

    // The name must be unique according to the HTML5 File System API spec.
    if (ContainsKey(file_system_names, filesystems[i].name)) {
      NOTREACHED() << "Duplicate file system name: " << filesystems[i].name;
      continue;
    }
    file_system_names.insert(filesystems[i].name);
    file_system_dict_value->SetStringWithoutPathExpansion(
        MediaFileSystemRegistry::kNameKey, filesystems[i].name);
    file_system_dict_value->SetStringWithoutPathExpansion(
        MediaFileSystemRegistry::kGalleryIdKey,
        base::Uint64ToString(filesystems[i].pref_id));
    if (!filesystems[i].transient_device_id.empty()) {
      file_system_dict_value->SetStringWithoutPathExpansion(
          MediaFileSystemRegistry::kDeviceIdKey,
          filesystems[i].transient_device_id);
    }
    file_system_dict_value->SetBooleanWithoutPathExpansion(
        kIsRemovableKey, filesystems[i].removable);
    file_system_dict_value->SetBooleanWithoutPathExpansion(
        kIsMediaDeviceKey, filesystems[i].media_device);

    list->Append(file_system_dict_value.release());

    if (filesystems[i].path.empty())
      continue;

    if (has_read_permission) {
      content::ChildProcessSecurityPolicy* policy =
          ChildProcessSecurityPolicy::GetInstance();
      if (!policy->CanReadFile(child_id, filesystems[i].path))
        policy->GrantReadFile(child_id, filesystems[i].path);
      policy->GrantReadFileSystem(child_id, filesystems[i].fsid);
    }
  }

  SetResult(list);
  SendResponse(true);
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialog() {
  WebContents* contents = WebContents::FromRenderViewHost(render_view_host());
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(contents);
  if (!web_contents_modal_dialog_manager) {
    // If there is no WebContentsModalDialogManager, then this contents is
    // probably the background page for an app. Try to find a shell window to
    // host the dialog.
    ShellWindow* window = ShellWindowRegistry::Get(profile())->
        GetCurrentShellWindowForApp(GetExtension()->id());
    if (window) {
      contents = window->web_contents();
    } else {
      // Abort showing the dialog. TODO(estade) Perhaps return an error instead.
      GetAndReturnGalleries();
      return;
    }
  }

  // Controller will delete itself.
  base::Closure cb = base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries, this);
  new chrome::MediaGalleriesDialogController(contents, *GetExtension(), cb);
}

void MediaGalleriesGetMediaFileSystemsFunction::GetMediaFileSystemsForExtension(
    const chrome::MediaFileSystemsCallback& cb) {
  if (!render_view_host()) {
    cb.Run(std::vector<MediaFileSystemInfo>());
    return;
  }

  MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  registry->GetMediaFileSystemsForExtension(
      render_view_host(), GetExtension(), cb);
}

// MediaGalleriesAssembleMediaFileFunction -------------------------------------

MediaGalleriesAssembleMediaFileFunction::
    ~MediaGalleriesAssembleMediaFileFunction() {}

bool MediaGalleriesAssembleMediaFileFunction::RunImpl() {
  if (!ApiIsAccessible(&error_))
    return false;

  // TODO(vandebo) Update the metadata and return the new file.
  SetResult(base::Value::CreateNullValue());
  return true;
}

}  // namespace extensions
