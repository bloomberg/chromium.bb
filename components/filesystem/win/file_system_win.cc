// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/win/file_system_win.h"

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "components/filesystem/posix/directory_posix.h"
#include "mojo/application/public/cpp/application_connection.h"

namespace filesystem {

FileSystemWin::FileSystemWin(mojo::ApplicationConnection* connection,
                                 mojo::InterfaceRequest<FileSystem> request)
    : remote_application_url_(connection->GetRemoteApplicationURL()),
      binding_(this, request.Pass()) {
}

FileSystemWin::~FileSystemWin() {
}

void FileSystemWin::OpenFileSystem(
    const mojo::String& file_system,
    mojo::InterfaceRequest<Directory> directory,
    const OpenFileSystemCallback& callback) {
  // TODO(erg): Fill this out.
  NOTIMPLEMENTED();
  callback.Run(ERROR_UNIMPLEMENTED);
}

}  // namespace filesystem

