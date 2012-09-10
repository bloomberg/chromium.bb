// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_galleries/media_galleries_api.h"

#include <string>
#include <vector>

#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/media_gallery/media_galleries_dialog_controller.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/extensions/api/experimental_media_galleries.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "chrome/common/extensions/permissions/media_galleries_permission.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_WIN)
#include "base/sys_string_conversions.h"
#endif

using content::WebContents;

namespace extensions {

namespace {

const char kDisallowedByPolicy[] =
    "Media Galleries API is disallowed by policy: ";
const char kInvalidInteractive[] = "Unknown value for interactive.";

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

using chrome::MediaFileSystemRegistry;
using content::ChildProcessSecurityPolicy;

namespace MediaGalleries = extensions::api::media_galleries;
namespace GetMediaFileSystems = MediaGalleries::GetMediaFileSystems;

MediaGalleriesGetMediaFileSystemsFunction::
    ~MediaGalleriesGetMediaFileSystemsFunction() {}

bool MediaGalleriesGetMediaFileSystemsFunction::RunImpl() {
  if (!ApiIsAccessible(&error_))
    return false;

  scoped_ptr<GetMediaFileSystems::Params> params(
      GetMediaFileSystems::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  MediaGalleries::GetMediaFileSystemsInteractivity interactive = "no";
  if (params->details.get() && params->details->interactive.get())
    interactive = *params->details->interactive;

  if (interactive == "yes") {
    ShowDialog();
    return true;
  } else if (interactive == "if_needed") {
    std::vector<MediaFileSystemRegistry::MediaFSInfo> filesystems =
        MediaFileSystemRegistry::GetInstance()->GetMediaFileSystemsForExtension(
            render_view_host(), GetExtension());
    if (filesystems.empty())
      ShowDialog();
    else
      ReturnGalleries(filesystems);

    return true;
  } else if (interactive == "no") {
    GetAndReturnGalleries();
    return true;
  }

  error_ = kInvalidInteractive;
  return false;
}

void MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries() {
  std::vector<MediaFileSystemRegistry::MediaFSInfo> filesystems =
      MediaFileSystemRegistry::GetInstance()->GetMediaFileSystemsForExtension(
          render_view_host(), GetExtension());
  ReturnGalleries(filesystems);
}

void MediaGalleriesGetMediaFileSystemsFunction::ReturnGalleries(
    const std::vector<MediaFileSystemRegistry::MediaFSInfo>& filesystems) {
  const int child_id = render_view_host()->GetProcess()->GetID();
  base::ListValue* list = new base::ListValue();
  for (size_t i = 0; i < filesystems.size(); i++) {
    base::DictionaryValue* dict_value = new base::DictionaryValue();
    dict_value->SetWithoutPathExpansion(
        "fsid", Value::CreateStringValue(filesystems[i].fsid));
    // The directory name is not exposed to the js layer.
    dict_value->SetWithoutPathExpansion(
        "name", Value::CreateStringValue(filesystems[i].name));
    list->Append(dict_value);

    if (!filesystems[i].path.empty() &&
        MediaGalleriesPermission::HasReadAccess(*GetExtension())) {
      content::ChildProcessSecurityPolicy* policy =
          ChildProcessSecurityPolicy::GetInstance();
      if (!policy->CanReadFile(child_id, filesystems[i].path))
        policy->GrantReadFile(child_id, filesystems[i].path);
      policy->GrantReadFileSystem(child_id, filesystems[i].fsid);
    }
    // TODO(vandebo) Handle write permission.
  }

  SetResult(list);
  SendResponse(true);
}

void MediaGalleriesGetMediaFileSystemsFunction::ShowDialog() {
  WebContents* contents = WebContents::FromRenderViewHost(render_view_host());
  TabContents* tab_contents =
      contents ? TabContents::FromWebContents(contents) : NULL;
  if (!tab_contents) {
    ShellWindow* window = ShellWindowRegistry::Get(profile())->
        GetCurrentShellWindowForApp(GetExtension()->id());
    if (window) {
      tab_contents = window->tab_contents();
    } else {
      // Abort showing the dialog. TODO(estade) Perhaps return an error instead.
      GetAndReturnGalleries();
      return;
    }
  }

  // Controller will delete itself.
  base::Closure cb = base::Bind(
      &MediaGalleriesGetMediaFileSystemsFunction::GetAndReturnGalleries, this);
  new chrome::MediaGalleriesDialogController(tab_contents, *GetExtension(), cb);
}

// MediaGalleriesAssembleMediaFileFunction -------------------------------------

MediaGalleriesAssembleMediaFileFunction::
    ~MediaGalleriesAssembleMediaFileFunction() {}

bool MediaGalleriesAssembleMediaFileFunction::RunImpl() {
  if (!ApiIsAccessible(&error_))
    return false;

  // TODO(vandebo) Update the metadata and return the new file.
  SetResult(Value::CreateNullValue());
  return true;
}

}  // namespace extensions
