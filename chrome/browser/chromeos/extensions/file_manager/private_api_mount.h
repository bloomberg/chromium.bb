// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_

#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"

namespace ui {
struct SelectedFileInfo;
}

namespace extensions {

// Implements chrome.fileManagerPrivate.addMount method.
// Mounts removable devices and archive files.
class FileManagerPrivateAddMountFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.addMount",
                             FILEMANAGERPRIVATE_ADDMOUNT)

 protected:
  virtual ~FileManagerPrivateAddMountFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;

 private:
  // Part of Run(). Called after MarkCacheFielAsMounted for Drive File System.
  // (or directly called from RunAsync() for other file system).
  void RunAfterMarkCacheFileAsMounted(const base::FilePath& display_name,
                                      drive::FileError error,
                                      const base::FilePath& file_path);
};

// Implements chrome.fileManagerPrivate.removeMount method.
// Unmounts selected volume. Expects volume id as an argument.
class FileManagerPrivateRemoveMountFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.removeMount",
                             FILEMANAGERPRIVATE_REMOVEMOUNT)

 protected:
  virtual ~FileManagerPrivateRemoveMountFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

// Implements chrome.fileManagerPrivate.getVolumeMetadataList method.
class FileManagerPrivateGetVolumeMetadataListFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getVolumeMetadataList",
                             FILEMANAGERPRIVATE_GETVOLUMEMETADATALIST)

 protected:
  virtual ~FileManagerPrivateGetVolumeMetadataListFunction() {}

  // AsyncExtensionFunction overrides.
  virtual bool RunAsync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
