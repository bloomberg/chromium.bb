// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INFO_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INFO_H_

#include <string>

#include "base/files/file_path.h"

namespace chromeos {
namespace file_system_provider {

// Contains information about the provided file system instance.
class ProvidedFileSystemInfo {
 public:
  ProvidedFileSystemInfo();
  ProvidedFileSystemInfo(const std::string& extension_id,
                         const std::string& file_system_id,
                         const std::string& display_name,
                         const bool writable,
                         const base::FilePath& mount_path);

  ~ProvidedFileSystemInfo();

  const std::string& extension_id() const { return extension_id_; }
  const std::string& file_system_id() const { return file_system_id_; }
  const std::string& display_name() const { return display_name_; }
  bool writable() const { return writable_; }
  const base::FilePath& mount_path() const { return mount_path_; }

 private:
  // ID of the extension providing this file system.
  std::string extension_id_;

  // ID of the file system.
  std::string file_system_id_;

  // Name of the file system, can be rendered in the UI.
  std::string display_name_;

  // Whether the file system is writable or just read-only.
  bool writable_;

  // Mount path of the underlying file system.
  base::FilePath mount_path_;
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INFO_H_
