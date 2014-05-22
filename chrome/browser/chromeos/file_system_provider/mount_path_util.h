// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "webkit/browser/fileapi/file_system_url.h"

class Profile;

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInterface;

namespace util {

// Constructs a safe mount point path for the provided file system.
base::FilePath GetMountPath(Profile* profile,
                            const std::string& extension_id,
                            const std::string& file_system_id);

// Finds file system, which is responsible for handling the specified |url| by
// analysing the mount path. Also, extract the file path from the virtual path
// to be used by the file system operations.
class FileSystemURLParser {
 public:
  explicit FileSystemURLParser(const fileapi::FileSystemURL& url);
  virtual ~FileSystemURLParser();

  // Parses the |url| passed to the constructor. If parsing succeeds, then
  // returns true. Otherwise, false. Must be called on UI thread.
  bool Parse();

  ProvidedFileSystemInterface* file_system() const { return file_system_; }
  const base::FilePath& file_path() const { return file_path_; }

 private:
  fileapi::FileSystemURL url_;
  ProvidedFileSystemInterface* file_system_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLParser);
};

}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_
