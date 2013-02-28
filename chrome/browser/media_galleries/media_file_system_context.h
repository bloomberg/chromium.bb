// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_

// Manages isolated filesystem namespaces for media file systems.

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/media_galleries/scoped_mtp_device_map_entry.h"
#include "webkit/fileapi/media/mtp_device_file_system_config.h"

namespace base {
class FilePath;
}

namespace chrome {

class MediaFileSystemRegistry;

class MediaFileSystemContext {
 public:
  virtual ~MediaFileSystemContext() {}

  // Register a media file system (filtered to media files) for |path| and
  // return the new file system id.
  virtual std::string RegisterFileSystemForMassStorage(
      const std::string& device_id, const base::FilePath& path) = 0;

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // Registers and returns the file system id for the MTP or PTP device
  // specified by |device_id| and |path|. Updates |entry| with the corresponding
  // ScopedMTPDeviceMapEntry object.
  virtual std::string RegisterFileSystemForMTPDevice(
      const std::string& device_id, const base::FilePath& path,
      scoped_refptr<ScopedMTPDeviceMapEntry>* entry) = 0;
#endif

  // Revoke the passed |fsid|.
  virtual void RevokeFileSystem(const std::string& fsid) = 0;

  // The MediaFileSystemRegistry that owns this context.
  virtual MediaFileSystemRegistry* GetMediaFileSystemRegistry() = 0;
};

}  // namespace

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_
