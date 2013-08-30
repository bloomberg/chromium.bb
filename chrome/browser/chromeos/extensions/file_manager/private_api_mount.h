// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides task related API functions.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "chrome/browser/chromeos/extensions/file_manager/private_api_base.h"

namespace ui {
struct SelectedFileInfo;
}

namespace extensions {

// Implements chrome.fileBrowserPrivate.addMount method.
// Mounts a device or a file.
class FileBrowserPrivateAddMountFunction : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.addMount",
                             FILEBROWSERPRIVATE_ADDMOUNT)

  FileBrowserPrivateAddMountFunction();

 protected:
  virtual ~FileBrowserPrivateAddMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of MarkCacheAsMounted.
  void OnMountedStateSet(const std::string& mount_type,
                         const base::FilePath::StringType& file_name,
                         drive::FileError error,
                         const base::FilePath& file_path);
};

// Implements chrome.fileBrowserPrivate.removeMount method.
// Unmounts selected device. Expects mount point path as an argument.
class FileBrowserPrivateRemoveMountFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.removeMount",
                             FILEBROWSERPRIVATE_REMOVEMOUNT)

  FileBrowserPrivateRemoveMountFunction();

 protected:
  virtual ~FileBrowserPrivateRemoveMountFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      const std::vector<ui::SelectedFileInfo>& files);
};

// Implements chrome.fileBrowserPrivate.getMountPoints method.
class FileBrowserPrivateGetMountPointsFunction
    : public LoggedAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileBrowserPrivate.getMountPoints",
                             FILEBROWSERPRIVATE_GETMOUNTPOINTS)

  FileBrowserPrivateGetMountPointsFunction();

 protected:
  virtual ~FileBrowserPrivateGetMountPointsFunction();

  // AsyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_PRIVATE_API_MOUNT_H_
