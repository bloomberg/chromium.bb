// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MediaFileSystemRegistry registers pictures directories as File API
// filesystems and keeps track of the path to filesystem ID mappings.
// In the near future, MediaFileSystemRegistry will also support media devices.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/lazy_instance.h"

class FilePath;

namespace fileapi {
class IsolatedContext;
}

namespace chrome {

class MediaFileSystemRegistry {
 public:
  // (Filesystem ID, path)
  typedef std::pair<std::string, FilePath> MediaFSIDAndPath;

  // The instance is lazily created per browser process.
  static MediaFileSystemRegistry* GetInstance();

  // Returns the list of media filesystem IDs and paths.
  // Called on the UI thread.
  std::vector<MediaFSIDAndPath> GetMediaFileSystems() const;

 private:
  friend struct base::DefaultLazyInstanceTraits<MediaFileSystemRegistry>;

  // Mapping of media directories to filesystem IDs.
  typedef std::map<FilePath, std::string> MediaPathToFSIDMap;

  // Obtain an instance of this class via GetInstance().
  MediaFileSystemRegistry();
  virtual ~MediaFileSystemRegistry();

  // Registers a path as a media file system.
  void RegisterPathAsFileSystem(const FilePath& path);

  // Only accessed on the UI thread.
  MediaPathToFSIDMap media_fs_map_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileSystemRegistry);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_MEDIA_FILE_SYSTEM_REGISTRY_H_
