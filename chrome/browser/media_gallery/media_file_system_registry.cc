// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry implementation.

#include "chrome/browser/media_gallery/media_file_system_registry.h"

#include <set>

#include "base/path_service.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/fileapi/isolated_context.h"

namespace chrome {

static base::LazyInstance<MediaFileSystemRegistry>::Leaky
    g_media_file_system_registry = LAZY_INSTANCE_INITIALIZER;

using content::BrowserThread;
using fileapi::IsolatedContext;

// static
MediaFileSystemRegistry* MediaFileSystemRegistry::GetInstance() {
  return g_media_file_system_registry.Pointer();
}

std::vector<MediaFileSystemRegistry::MediaFSIDAndPath>
MediaFileSystemRegistry::GetMediaFileSystems() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<MediaFSIDAndPath> results;
  for (MediaPathToFSIDMap::const_iterator it = media_fs_map_.begin();
       it != media_fs_map_.end();
       ++it) {
    const FilePath path = it->first;
    const std::string fsid = it->second;
    results.push_back(std::make_pair(fsid, path));
  }
  return results;
}

MediaFileSystemRegistry::MediaFileSystemRegistry() {
  FilePath pictures_path;
  if (PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path)) {
    RegisterPathAsFileSystem(pictures_path);
  }
}

MediaFileSystemRegistry::~MediaFileSystemRegistry() {
}

void MediaFileSystemRegistry::RegisterPathAsFileSystem(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Sanity checks for |path|.
  CHECK(path.IsAbsolute());
  CHECK(!path.ReferencesParent());
  // Make sure |path| does not refer to '/' on Unix.
  // TODO(thestig) Check how BaseName() works for say, 'C:\' on Windows.
  CHECK(!path.BaseName().IsAbsolute());
  CHECK(!path.BaseName().empty());

  std::set<FilePath> fileset;
  fileset.insert(path);
  const std::string fsid =
      IsolatedContext::GetInstance()->RegisterIsolatedFileSystem(fileset);
  CHECK(!fsid.empty());
  media_fs_map_.insert(std::make_pair(path, fsid));
}

}  // namespace chrome
