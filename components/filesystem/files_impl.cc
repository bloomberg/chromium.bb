// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/files_impl.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "components/filesystem/directory_impl.h"

namespace mojo {
namespace files {

namespace {

base::ScopedFD CreateAndOpenTemporaryDirectory(
    scoped_ptr<base::ScopedTempDir>* temp_dir) {
  (*temp_dir).reset(new base::ScopedTempDir());
  CHECK((*temp_dir)->CreateUniqueTempDir());

  base::ScopedFD temp_dir_fd(HANDLE_EINTR(
      open((*temp_dir)->path().value().c_str(), O_RDONLY | O_DIRECTORY, 0)));
  PCHECK(temp_dir_fd.is_valid());
  DVLOG(1) << "Made a temporary directory: " << (*temp_dir)->path().value();
  return temp_dir_fd.Pass();
}

#ifndef NDEBUG
base::ScopedFD OpenMojoDebugDirectory() {
  const char* home_dir_name = getenv("HOME");
  if (!home_dir_name || !home_dir_name[0]) {
    LOG(ERROR) << "HOME not set";
    return base::ScopedFD();
  }
  base::FilePath mojo_debug_dir_name =
      base::FilePath(home_dir_name).Append("MojoDebug");
  return base::ScopedFD(HANDLE_EINTR(
      open(mojo_debug_dir_name.value().c_str(), O_RDONLY | O_DIRECTORY, 0)));
}
#endif

}  // namespace

FilesImpl::FilesImpl(ApplicationConnection* connection,
                     InterfaceRequest<Files> request)
    : binding_(this, request.Pass()) {
  // TODO(vtl): record other app's URL
}

FilesImpl::~FilesImpl() {
}

void FilesImpl::OpenFileSystem(const mojo::String& file_system,
                               InterfaceRequest<Directory> directory,
                               const OpenFileSystemCallback& callback) {
  base::ScopedFD dir_fd;
  // Set only if the |DirectoryImpl| will own a temporary directory.
  scoped_ptr<base::ScopedTempDir> temp_dir;
  if (file_system.is_null()) {
    // TODO(vtl): ScopedGeneric (hence ScopedFD) doesn't have an operator=!
    dir_fd.reset(CreateAndOpenTemporaryDirectory(&temp_dir).release());
    DCHECK(temp_dir);
  } else if (file_system.get() == std::string("debug")) {
#ifdef NDEBUG
    LOG(WARNING) << "~/MojoDebug only available in Debug builds";
#else
    // TODO(vtl): ScopedGeneric (hence ScopedFD) doesn't have an operator=!
    dir_fd.reset(OpenMojoDebugDirectory().release());
#endif
    if (!dir_fd.is_valid()) {
      LOG(ERROR) << "~/MojoDebug unavailable";
      callback.Run(ERROR_UNAVAILABLE);
      return;
    }
  } else {
    LOG(ERROR) << "Unknown file system: " << file_system.get();
    callback.Run(ERROR_UNIMPLEMENTED);
    return;
  }

  new DirectoryImpl(directory.Pass(), dir_fd.Pass(), temp_dir.Pass());
  callback.Run(ERROR_OK);
}

}  // namespace files
}  // namespace mojo
