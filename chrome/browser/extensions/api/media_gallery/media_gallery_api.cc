// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Media Galleries API.

#include "chrome/browser/extensions/api/media_gallery/media_gallery_api.h"

#include <string>
#include <vector>

#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"

#if defined(OS_WIN)
#include "base/sys_string_conversions.h"
#endif

namespace extensions {

using chrome::MediaFileSystemRegistry;
using content::ChildProcessSecurityPolicy;

GetMediaFileSystemsFunction::~GetMediaFileSystemsFunction() {}

bool GetMediaFileSystemsFunction::RunImpl() {
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
    const std::string basepath_utf8(path.BaseName().AsUTF8Unsafe());

    base::DictionaryValue* dict_value = new base::DictionaryValue();
    dict_value->SetWithoutPathExpansion(
        "fsid", Value::CreateStringValue(fsid));
    dict_value->SetWithoutPathExpansion(
        "dirname", Value::CreateStringValue(basepath_utf8));
    list->Append(dict_value);

    content::ChildProcessSecurityPolicy* policy =
        ChildProcessSecurityPolicy::GetInstance();
    if (!policy->CanReadFile(child_id, path))
      policy->GrantReadFile(child_id, path);
    policy->GrantReadFileSystem(child_id, fsid);
  }

  result_.reset(list);
  return true;
}

OpenMediaGalleryManagerFunction::~OpenMediaGalleryManagerFunction() {}

bool OpenMediaGalleryManagerFunction::RunImpl() {
  // TODO(vandebo) Open the Media Gallery Manager UI.
  result_.reset(Value::CreateNullValue());
  return true;
}

AssembleMediaFileFunction::~AssembleMediaFileFunction() {}

bool AssembleMediaFileFunction::RunImpl() {
  // TODO(vandebo) Update the metadata and return the new file.
  result_.reset(Value::CreateNullValue());
  return true;
}

}  // namespace extensions
