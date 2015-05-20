// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/directory_impl.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "components/filesystem/file_impl.h"
#include "components/filesystem/shared_impl.h"
#include "components/filesystem/util.h"

namespace mojo {
namespace files {

namespace {

// Calls |closedir()| on a |DIR*|.
struct DIRDeleter {
  void operator()(DIR* dir) const { PCHECK(closedir(dir) == 0); }
};
using ScopedDIR = scoped_ptr<DIR, DIRDeleter>;

Error ValidateOpenFlags(uint32_t open_flags, bool is_directory) {
  // Treat unknown flags as "unimplemented".
  if ((open_flags &
       ~(kOpenFlagRead | kOpenFlagWrite | kOpenFlagCreate | kOpenFlagExclusive |
         kOpenFlagAppend | kOpenFlagTruncate)))
    return ERROR_UNIMPLEMENTED;

  // At least one of |kOpenFlagRead| or |kOpenFlagWrite| must be set.
  if (!(open_flags & (kOpenFlagRead | kOpenFlagWrite)))
    return ERROR_INVALID_ARGUMENT;

  // |kOpenFlagCreate| requires |kOpenFlagWrite|.
  if ((open_flags & kOpenFlagCreate) && !(open_flags & kOpenFlagWrite))
    return ERROR_INVALID_ARGUMENT;

  // |kOpenFlagExclusive| requires |kOpenFlagCreate|.
  if ((open_flags & kOpenFlagExclusive) && !(open_flags & kOpenFlagCreate))
    return ERROR_INVALID_ARGUMENT;

  if (is_directory) {
    // Check that file-only flags aren't set.
    if ((open_flags & (kOpenFlagAppend | kOpenFlagTruncate)))
      return ERROR_INVALID_ARGUMENT;
    return ERROR_OK;
  }

  // File-only flags:

  // |kOpenFlagAppend| requires |kOpenFlagWrite|.
  if ((open_flags & kOpenFlagAppend) && !(open_flags & kOpenFlagWrite))
    return ERROR_INVALID_ARGUMENT;

  // |kOpenFlagTruncate| requires |kOpenFlagWrite|.
  if ((open_flags & kOpenFlagTruncate) && !(open_flags & kOpenFlagWrite))
    return ERROR_INVALID_ARGUMENT;

  return ERROR_OK;
}

Error ValidateDeleteFlags(uint32_t delete_flags) {
  // Treat unknown flags as "unimplemented".
  if ((delete_flags &
       ~(kDeleteFlagFileOnly | kDeleteFlagDirectoryOnly |
         kDeleteFlagRecursive)))
    return ERROR_UNIMPLEMENTED;

  // Only one of the three currently-defined flags may be set.
  if ((delete_flags & kDeleteFlagFileOnly) &&
      (delete_flags & (kDeleteFlagDirectoryOnly | kDeleteFlagRecursive)))
    return ERROR_INVALID_ARGUMENT;
  if ((delete_flags & kDeleteFlagDirectoryOnly) &&
      (delete_flags & (kDeleteFlagFileOnly | kDeleteFlagRecursive)))
    return ERROR_INVALID_ARGUMENT;
  if ((delete_flags & kDeleteFlagRecursive) &&
      (delete_flags & (kDeleteFlagFileOnly | kDeleteFlagDirectoryOnly)))
    return ERROR_INVALID_ARGUMENT;

  return ERROR_OK;
}

}  // namespace

DirectoryImpl::DirectoryImpl(InterfaceRequest<Directory> request,
                             base::ScopedFD dir_fd,
                             scoped_ptr<base::ScopedTempDir> temp_dir)
    : binding_(this, request.Pass()),
      dir_fd_(dir_fd.Pass()),
      temp_dir_(temp_dir.Pass()) {
  DCHECK(dir_fd_.is_valid());
}

DirectoryImpl::~DirectoryImpl() {
}

void DirectoryImpl::Read(const ReadCallback& callback) {
  static const size_t kMaxReadCount = 1000;

  DCHECK(dir_fd_.is_valid());

  // |fdopendir()| takes ownership of the FD (giving it to the |DIR| --
  // |closedir()| will close the FD)), so we need to |dup()| ours.
  base::ScopedFD fd(dup(dir_fd_.get()));
  if (!fd.is_valid()) {
    callback.Run(ErrnoToError(errno), Array<DirectoryEntryPtr>());
    return;
  }

  ScopedDIR dir(fdopendir(fd.release()));
  if (!dir) {
    callback.Run(ErrnoToError(errno), Array<DirectoryEntryPtr>());
    return;
  }

  Array<DirectoryEntryPtr> result(0);

// Warning: This is not portable (per POSIX.1 -- |buffer| may not be large
// enough), but it's fine for Linux.
#if !defined(OS_ANDROID) && !defined(OS_LINUX)
#error "Use of struct dirent for readdir_r() buffer not portable; please check."
#endif
  struct dirent buffer;
  for (size_t n = 0;;) {
    struct dirent* entry = nullptr;
    if (int error = readdir_r(dir.get(), &buffer, &entry)) {
      // |error| is effectively an errno (for |readdir_r()|), AFAICT.
      callback.Run(ErrnoToError(error), Array<DirectoryEntryPtr>());
      return;
    }

    if (!entry)
      break;

    n++;
    if (n > kMaxReadCount) {
      LOG(WARNING) << "Directory contents truncated";
      callback.Run(ERROR_OUT_OF_RANGE, result.Pass());
      return;
    }

    DirectoryEntryPtr e = DirectoryEntry::New();
    switch (entry->d_type) {
      case DT_DIR:
        e->type = FILE_TYPE_DIRECTORY;
        break;
      case DT_REG:
        e->type = FILE_TYPE_REGULAR_FILE;
        break;
      default:
        e->type = FILE_TYPE_UNKNOWN;
        break;
    }
    e->name = String(entry->d_name);
    result.push_back(e.Pass());
  }

  callback.Run(ERROR_OK, result.Pass());
}

void DirectoryImpl::Stat(const StatCallback& callback) {
  DCHECK(dir_fd_.is_valid());
  StatFD(dir_fd_.get(), FILE_TYPE_DIRECTORY, callback);
}

void DirectoryImpl::Touch(TimespecOrNowPtr atime,
                          TimespecOrNowPtr mtime,
                          const TouchCallback& callback) {
  DCHECK(dir_fd_.is_valid());
  TouchFD(dir_fd_.get(), atime.Pass(), mtime.Pass(), callback);
}

// TODO(vtl): Move the implementation to a thread pool.
void DirectoryImpl::OpenFile(const String& path,
                             InterfaceRequest<File> file,
                             uint32_t open_flags,
                             const OpenFileCallback& callback) {
  DCHECK(!path.is_null());
  DCHECK(dir_fd_.is_valid());

  if (Error error = IsPathValid(path)) {
    callback.Run(error);
    return;
  }
  // TODO(vtl): Make sure the path doesn't exit this directory (if appropriate).
  // TODO(vtl): Maybe allow absolute paths?

  if (Error error = ValidateOpenFlags(open_flags, false)) {
    callback.Run(error);
    return;
  }

  int flags = 0;
  if ((open_flags & kOpenFlagRead))
    flags |= (open_flags & kOpenFlagWrite) ? O_RDWR : O_RDONLY;
  else
    flags |= O_WRONLY;
  if ((open_flags & kOpenFlagCreate))
    flags |= O_CREAT;
  if ((open_flags & kOpenFlagExclusive))
    flags |= O_EXCL;
  if ((open_flags & kOpenFlagAppend))
    flags |= O_APPEND;
  if ((open_flags & kOpenFlagTruncate))
    flags |= O_TRUNC;

  base::ScopedFD file_fd(
      HANDLE_EINTR(openat(dir_fd_.get(), path.get().c_str(), flags, 0600)));
  if (!file_fd.is_valid()) {
    callback.Run(ErrnoToError(errno));
    return;
  }

  if (file.is_pending())
    new FileImpl(file.Pass(), file_fd.Pass());
  callback.Run(ERROR_OK);
}

void DirectoryImpl::OpenDirectory(const String& path,
                                  InterfaceRequest<Directory> directory,
                                  uint32_t open_flags,
                                  const OpenDirectoryCallback& callback) {
  DCHECK(!path.is_null());
  DCHECK(dir_fd_.is_valid());

  if (Error error = IsPathValid(path)) {
    callback.Run(error);
    return;
  }
  // TODO(vtl): Make sure the path doesn't exit this directory (if appropriate).
  // TODO(vtl): Maybe allow absolute paths?

  if (Error error = ValidateOpenFlags(open_flags, false)) {
    callback.Run(error);
    return;
  }

  // TODO(vtl): Implement read-only (whatever that means).
  if (!(open_flags & kOpenFlagWrite)) {
    callback.Run(ERROR_UNIMPLEMENTED);
    return;
  }

  if ((open_flags & kOpenFlagCreate)) {
    if (mkdirat(dir_fd_.get(), path.get().c_str(), 0700) != 0) {
      // Allow |EEXIST| if |kOpenFlagExclusive| is not set. Note, however, that
      // it does not guarantee that |path| is a directory.
      // TODO(vtl): Hrm, ponder if we should check that |path| is a directory.
      if (errno != EEXIST || !(open_flags & kOpenFlagExclusive)) {
        callback.Run(ErrnoToError(errno));
        return;
      }
    }
  }

  base::ScopedFD new_dir_fd(
      HANDLE_EINTR(openat(dir_fd_.get(), path.get().c_str(), O_DIRECTORY, 0)));
  if (!new_dir_fd.is_valid()) {
    callback.Run(ErrnoToError(errno));
    return;
  }

  if (directory.is_pending())
    new DirectoryImpl(directory.Pass(), new_dir_fd.Pass(), nullptr);
  callback.Run(ERROR_OK);
}

void DirectoryImpl::Rename(const String& path,
                           const String& new_path,
                           const RenameCallback& callback) {
  DCHECK(!path.is_null());
  DCHECK(!new_path.is_null());
  DCHECK(dir_fd_.is_valid());

  if (Error error = IsPathValid(path)) {
    callback.Run(error);
    return;
  }
  if (Error error = IsPathValid(new_path)) {
    callback.Run(error);
    return;
  }
  // TODO(vtl): See TODOs about |path| in OpenFile().

  if (renameat(dir_fd_.get(), path.get().c_str(), dir_fd_.get(),
               new_path.get().c_str())) {
    callback.Run(ErrnoToError(errno));
    return;
  }

  callback.Run(ERROR_OK);
}

void DirectoryImpl::Delete(const String& path,
                           uint32_t delete_flags,
                           const DeleteCallback& callback) {
  DCHECK(!path.is_null());
  DCHECK(dir_fd_.is_valid());

  if (Error error = IsPathValid(path)) {
    callback.Run(error);
    return;
  }
  // TODO(vtl): See TODOs about |path| in OpenFile().

  if (Error error = ValidateDeleteFlags(delete_flags)) {
    callback.Run(error);
    return;
  }

  // TODO(vtl): Recursive not yet supported.
  if ((delete_flags & kDeleteFlagRecursive)) {
    callback.Run(ERROR_UNIMPLEMENTED);
    return;
  }

  // First try deleting it as a file, unless we're told to do directory-only.
  if (!(delete_flags & kDeleteFlagDirectoryOnly)) {
    if (unlinkat(dir_fd_.get(), path.get().c_str(), 0) == 0) {
      callback.Run(ERROR_OK);
      return;
    }

    // If file-only, don't continue.
    if ((delete_flags & kDeleteFlagFileOnly)) {
      callback.Run(ErrnoToError(errno));
      return;
    }
  }

  // Try deleting it as a directory.
  if (unlinkat(dir_fd_.get(), path.get().c_str(), AT_REMOVEDIR) == 0) {
    callback.Run(ERROR_OK);
    return;
  }

  callback.Run(ErrnoToError(errno));
}

}  // namespace files
}  // namespace mojo
