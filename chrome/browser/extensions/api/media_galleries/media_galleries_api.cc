// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_galleries/media_galleries_api.h"

#include <string>
#include <vector>

#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/common/extensions/api/experimental_media_galleries.h"
#include "chrome/common/extensions/api/media_galleries.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

#if defined(OS_WIN)
#include "base/sys_string_conversions.h"
#endif

namespace extensions {

namespace {

const char kInvalidInteractive[] = "Unknown value for interactive.";

}  // namespace

using chrome::MediaFileSystemRegistry;
using content::ChildProcessSecurityPolicy;

namespace MediaGalleries = extensions::api::media_galleries;
namespace GetMediaFileSystems = MediaGalleries::GetMediaFileSystems;

MediaGalleriesGetMediaFileSystemsFunction::
    ~MediaGalleriesGetMediaFileSystemsFunction() {}

bool MediaGalleriesGetMediaFileSystemsFunction::RunImpl() {
  scoped_ptr<GetMediaFileSystems::Params> params(
      GetMediaFileSystems::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  MediaGalleries::GetMediaFileSystemsInteractivity interactive = "no";
  if (params->details.get() && params->details->interactive.get())
    interactive = *params->details->interactive;

  if (interactive == "yes") {
    // TODO(estade): implement.
  } else if (interactive == "if_needed") {
    // TODO(estade): implement.
  } else if (interactive != "no") {
    error_ = kInvalidInteractive;
    return false;
  }

  const content::RenderProcessHost* rph = render_view_host()->GetProcess();
  chrome::MediaFileSystemRegistry* media_fs_registry =
      MediaFileSystemRegistry::GetInstance();
  const std::vector<MediaFileSystemRegistry::MediaFSIDAndPath> filesystems =
      media_fs_registry->GetMediaFileSystems(rph);

  const int child_id = rph->GetID();
  base::ListValue* list = new base::ListValue();
  for (size_t i = 0; i < filesystems.size(); i++) {
    // TODO(thestig) Check permissions to file systems when that capability
    // exists.
    const MediaFileSystemRegistry::MediaFSIDAndPath& fsid_and_path =
        filesystems[i];
    const std::string& fsid = fsid_and_path.first;
    const FilePath& path = fsid_and_path.second;

    base::DictionaryValue* dict_value = new base::DictionaryValue();
    dict_value->SetWithoutPathExpansion(
        "fsid", Value::CreateStringValue(fsid));
    // The directory name is not exposed to the js layer.
    dict_value->SetWithoutPathExpansion(
        "dirname", Value::CreateStringValue("_"));
    list->Append(dict_value);


    if (GetExtension()->HasAPIPermission(
            extensions::APIPermission::kMediaGalleriesRead)) {
      content::ChildProcessSecurityPolicy* policy =
          ChildProcessSecurityPolicy::GetInstance();
      if (!policy->CanReadFile(child_id, path))
        policy->GrantReadFile(child_id, path);
      policy->GrantReadFileSystem(child_id, fsid);
    }
    // TODO(vandebo) Handle write permission.
  }

  SetResult(list);
  return true;
}

MediaGalleriesAssembleMediaFileFunction::
    ~MediaGalleriesAssembleMediaFileFunction() {}

bool MediaGalleriesAssembleMediaFileFunction::RunImpl() {
  // TODO(vandebo) Update the metadata and return the new file.
  SetResult(Value::CreateNullValue());
  return true;
}

}  // namespace extensions
