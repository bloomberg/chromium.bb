// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/file_system_impl.h"

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/filesystem/directory_impl.h"
#include "mojo/application/public/cpp/application_connection.h"

namespace filesystem {

FileSystemImpl::FileSystemImpl(mojo::ApplicationConnection* connection,
                               mojo::InterfaceRequest<FileSystem> request)
    : remote_application_url_(connection->GetRemoteApplicationURL()),
      binding_(this, request.Pass()) {
}

FileSystemImpl::~FileSystemImpl() {
}

void FileSystemImpl::OpenFileSystem(const mojo::String& file_system,
                                    mojo::InterfaceRequest<Directory> directory,
                                    const OpenFileSystemCallback& callback) {
  // Set only if the |DirectoryImpl| will own a temporary directory.
  scoped_ptr<base::ScopedTempDir> temp_dir;
  base::FilePath path;
  if (file_system.get() == std::string("temp")) {
    temp_dir.reset(new base::ScopedTempDir);
    CHECK(temp_dir->CreateUniqueTempDir());
    path = temp_dir->path();
  } else if (file_system.get() == std::string("origin")) {
    // TODO(erg): We should serve a persistent directory based on the
    // subdirectory |remote_application_url_| of a profile directory.
  }

  if (!path.empty()) {
    new DirectoryImpl(directory.Pass(), path, temp_dir.Pass());
    callback.Run(FILE_ERROR_OK);
  } else {
    callback.Run(FILE_ERROR_FAILED);
  }
}

}  // namespace filesystem
