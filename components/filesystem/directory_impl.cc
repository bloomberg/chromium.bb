// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/filesystem/directory_impl.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "components/filesystem/file_impl.h"
#include "components/filesystem/util.h"

namespace filesystem {

DirectoryImpl::DirectoryImpl(mojo::InterfaceRequest<Directory> request,
                             base::FilePath directory_path,
                             scoped_ptr<base::ScopedTempDir> temp_dir)
    : binding_(this, std::move(request)),
      directory_path_(directory_path),
      temp_dir_(std::move(temp_dir)) {}

DirectoryImpl::~DirectoryImpl() {
}

void DirectoryImpl::Read(const ReadCallback& callback) {
  mojo::Array<DirectoryEntryPtr> entries(0);
  base::FileEnumerator directory_enumerator(
      directory_path_, false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath name = directory_enumerator.Next(); !name.empty();
       name = directory_enumerator.Next()) {
    base::FileEnumerator::FileInfo info = directory_enumerator.GetInfo();
    DirectoryEntryPtr entry = DirectoryEntry::New();
    entry->type =
        info.IsDirectory() ? FsFileType::DIRECTORY : FsFileType::REGULAR_FILE;
    entry->name = info.GetName().AsUTF8Unsafe();
    entries.push_back(std::move(entry));
  }

  callback.Run(FileError::OK, std::move(entries));
}

// TODO(erg): Consider adding an implementation of Stat()/Touch() to the
// directory, too. Right now, the base::File abstractions do not really deal
// with directories properly, so these are broken for now.

// TODO(vtl): Move the implementation to a thread pool.
void DirectoryImpl::OpenFile(const mojo::String& raw_path,
                             mojo::InterfaceRequest<File> file,
                             uint32_t open_flags,
                             const OpenFileCallback& callback) {
  base::FilePath path;
  FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != FileError::OK) {
    callback.Run(error);
    return;
  }

#if defined(OS_WIN)
  // On Windows, FILE_FLAG_BACKUP_SEMANTICS is needed to open a directory.
  if (DirectoryExists(path))
    open_flags |= base::File::FLAG_BACKUP_SEMANTICS;
#endif  // OS_WIN

  base::File base_file(path, open_flags);
  if (!base_file.IsValid()) {
    callback.Run(GetError(base_file));
    return;
  }

  base::File::Info info;
  if (!base_file.GetInfo(&info)) {
    callback.Run(FileError::FAILED);
    return;
  }

  if (info.is_directory) {
    // We must not return directories as files. In the file abstraction, we can
    // fetch raw file descriptors over mojo pipes, and passing a file
    // descriptor to a directory is a sandbox escape on Windows.
    callback.Run(FileError::NOT_A_FILE);
    return;
  }

  if (file.is_pending()) {
    new FileImpl(std::move(file), std::move(base_file));
  }
  callback.Run(FileError::OK);
}

void DirectoryImpl::OpenDirectory(const mojo::String& raw_path,
                                  mojo::InterfaceRequest<Directory> directory,
                                  uint32_t open_flags,
                                  const OpenDirectoryCallback& callback) {
  base::FilePath path;
  FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != FileError::OK) {
    callback.Run(error);
    return;
  }

  if (!base::DirectoryExists(path)) {
    if (base::PathExists(path)) {
      callback.Run(FileError::NOT_A_DIRECTORY);
      return;
    }

    if (!(open_flags & kFlagOpenAlways || open_flags & kFlagCreate)) {
      // The directory doesn't exist, and we weren't passed parameters to
      // create it.
      callback.Run(FileError::NOT_FOUND);
      return;
    }

    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(path, &error)) {
      callback.Run(static_cast<filesystem::FileError>(error));
      return;
    }
  }

  if (directory.is_pending())
    new DirectoryImpl(std::move(directory), path,
                      scoped_ptr<base::ScopedTempDir>());
  callback.Run(FileError::OK);
}

void DirectoryImpl::Rename(const mojo::String& raw_old_path,
                           const mojo::String& raw_new_path,
                           const RenameCallback& callback) {
  base::FilePath old_path;
  FileError error = ValidatePath(raw_old_path, directory_path_, &old_path);
  if (error != FileError::OK) {
    callback.Run(error);
    return;
  }

  base::FilePath new_path;
  error = ValidatePath(raw_new_path, directory_path_, &new_path);
  if (error != FileError::OK) {
    callback.Run(error);
    return;
  }

  if (!base::Move(old_path, new_path)) {
    callback.Run(FileError::FAILED);
    return;
  }

  callback.Run(FileError::OK);
}

void DirectoryImpl::Delete(const mojo::String& raw_path,
                           uint32_t delete_flags,
                           const DeleteCallback& callback) {
  base::FilePath path;
  FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != FileError::OK) {
    callback.Run(error);
    return;
  }

  bool recursive = delete_flags & kDeleteFlagRecursive;
  if (!base::DeleteFile(path, recursive)) {
    callback.Run(FileError::FAILED);
    return;
  }

  callback.Run(FileError::OK);
}

void DirectoryImpl::Exists(const mojo::String& raw_path,
                           const ExistsCallback& callback) {
  base::FilePath path;
  FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != FileError::OK) {
    callback.Run(error, false);
    return;
  }

  bool exists = base::PathExists(path);
  callback.Run(FileError::OK, exists);
}

void DirectoryImpl::IsWritable(const mojo::String& raw_path,
                               const IsWritableCallback& callback) {
  base::FilePath path;
  FileError error = ValidatePath(raw_path, directory_path_, &path);
  if (error != FileError::OK) {
    callback.Run(error, false);
    return;
  }

  callback.Run(FileError::OK, base::PathIsWritable(path));
}

void DirectoryImpl::Flush(const FlushCallback& callback) {
  base::File file(directory_path_, base::File::FLAG_READ);
  if (!file.IsValid()) {
    callback.Run(GetError(file));
    return;
  }

  if (!file.Flush()) {
    callback.Run(FileError::FAILED);
    return;
  }

  callback.Run(FileError::OK);
}

}  // namespace filesystem
