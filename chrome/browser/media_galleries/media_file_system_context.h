// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_

// Manages isolated filesystem namespaces for media file systems.

#include <string>

namespace base {
class FilePath;
}

class MediaFileSystemRegistry;

class MediaFileSystemContext {
 public:
  virtual ~MediaFileSystemContext() {}

  // Register a media file system (filtered to media files) for |path| and
  // return the new file system id.
  virtual std::string RegisterFileSystemForMassStorage(
      const std::string& device_id, const base::FilePath& path) = 0;

  // Registers and returns the file system id for the MTP or PTP device
  // specified by |device_id| and |path|.
  virtual std::string RegisterFileSystemForMTPDevice(
      const std::string& device_id, const base::FilePath& path) = 0;

  // Revoke the passed |fsid|.
  virtual void RevokeFileSystem(const std::string& fsid) = 0;

  // Signal the registry that a particular MTP device map entry is no longer
  // needed.
  virtual void RemoveScopedMTPDeviceMapEntry(
      const base::FilePath::StringType& device_location) = 0;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_
