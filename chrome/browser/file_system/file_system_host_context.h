// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "webkit/fileapi/file_system_types.h"

class GURL;

// This is owned by profile and shared by all the FileSystemDispatcherHost
// that shared by the same profile.
class FileSystemHostContext
    : public base::RefCountedThreadSafe<FileSystemHostContext> {
 public:
  FileSystemHostContext(const FilePath& data_path, bool is_incognito);
  const FilePath& base_path() const { return base_path_; }
  bool is_incognito() const { return is_incognito_; }

  // Returns the root path and name for the file system specified by given
  // |origin_url| and |type|.  Returns true if the file system is available
  // for the profile and |root_path| and |name| are filled successfully.
  bool GetFileSystemRootPath(const GURL& origin_url,
                             fileapi::FileSystemType type,
                             FilePath* root_path,
                             std::string* name) const;

  // Check if the given |path| is in the FileSystem base directory.
  bool CheckValidFileSystemPath(const FilePath& path) const;

  // Returns the storage identifier string for the given |url|.
  static std::string GetStorageIdentifierFromURL(const GURL& url);

  // The FileSystem directory name.
  static const FilePath::CharType kFileSystemDirectory[];

  static const char kPersistentName[];
  static const char kTemporaryName[];

 private:
  const FilePath base_path_;
  const bool is_incognito_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemHostContext);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_
