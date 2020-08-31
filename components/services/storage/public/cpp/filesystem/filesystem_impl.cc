// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/filesystem/filesystem_impl.h"

#include <set>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace storage {

namespace {

// Retains a mapping of lock file paths which have been locked by
// |FilesystemImpl::LockFile| and not yet released.
class LockTable {
 public:
  LockTable() = default;
  LockTable(const LockTable&) = delete;
  LockTable& operator=(const LockTable&) = delete;
  ~LockTable() = default;

  bool AddLock(const base::FilePath& path) {
    DCHECK(path.IsAbsolute());
    base::AutoLock lock(lock_);
    auto result = lock_paths_.insert(path.NormalizePathSeparators());
    return result.second;
  }

  void RemoveLock(const base::FilePath& path) {
    const base::FilePath normalized_path = path.NormalizePathSeparators();
    base::AutoLock lock(lock_);
    DCHECK(base::Contains(lock_paths_, normalized_path));
    lock_paths_.erase(normalized_path);
  }

 private:
  base::Lock lock_;
  std::set<base::FilePath> lock_paths_ GUARDED_BY(lock_);
};

// Get the global singleton instance of LockTable. This returned object is
// thread-safe.
LockTable& GetLockTable() {
  static base::NoDestructor<LockTable> table;
  return *table;
}

class FileLockImpl : public mojom::FileLock {
 public:
  FileLockImpl(const base::FilePath& path, base::File file)
      : path_(path), file_(std::move(file)) {
    DCHECK(file_.IsValid());
  }

  ~FileLockImpl() override {
    if (file_.IsValid())
      GetLockTable().RemoveLock(path_);
  }

  // mojom::FileLock implementation:
  void Release(ReleaseCallback callback) override {
    if (!file_.IsValid()) {
      std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
      return;
    }

#if defined(OS_FUCHSIA)
    std::move(callback).Run(base::File::FILE_OK);
#else
    std::move(callback).Run(file_.Unlock());
#endif
    GetLockTable().RemoveLock(path_);
    file_.Close();
  }

 private:
  const base::FilePath path_;
  base::File file_;
};

}  // namespace

FilesystemImpl::FilesystemImpl(const base::FilePath& root) : root_(root) {}

FilesystemImpl::~FilesystemImpl() = default;

void FilesystemImpl::Clone(mojo::PendingReceiver<mojom::Directory> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FilesystemImpl>(root_),
                              std::move(receiver));
}

void FilesystemImpl::PathExists(const base::FilePath& path,
                                PathExistsCallback callback) {
  std::move(callback).Run(base::PathExists(MakeAbsolute(path)));
}

void FilesystemImpl::GetEntries(const base::FilePath& path,
                                mojom::GetEntriesMode mode,
                                GetEntriesCallback callback) {
  const base::FilePath full_path = MakeAbsolute(path);
  FileErrorOr<std::vector<base::FilePath>> result =
      GetDirectoryEntries(full_path, mode);
  if (result.is_error()) {
    std::move(callback).Run(result.error(), std::vector<base::FilePath>());
    return;
  }

  // Fix up the absolute paths to be relative to |path|.
  std::vector<base::FilePath> entries;
  std::vector<base::FilePath::StringType> root_components;
  full_path.GetComponents(&root_components);
  const size_t num_components_to_strip = root_components.size();
  for (const auto& entry : result.value()) {
    std::vector<base::FilePath::StringType> components;
    entry.GetComponents(&components);
    base::FilePath relative_path;
    for (size_t i = num_components_to_strip; i < components.size(); ++i)
      relative_path = relative_path.Append(components[i]);
    entries.push_back(std::move(relative_path));
  }
  std::move(callback).Run(base::File::FILE_OK, entries);
}

void FilesystemImpl::OpenFile(const base::FilePath& path,
                              mojom::FileOpenMode mode,
                              mojom::FileReadAccess read_access,
                              mojom::FileWriteAccess write_access,
                              OpenFileCallback callback) {
  uint32_t flags = 0;
  switch (mode) {
    case mojom::FileOpenMode::kOpenIfExists:
      flags |= base::File::FLAG_OPEN;
      break;
    case mojom::FileOpenMode::kCreateAndOpenOnlyIfNotExists:
      flags |= base::File::FLAG_CREATE;
      break;
    case mojom::FileOpenMode::kAlwaysOpen:
      flags |= base::File::FLAG_OPEN_ALWAYS;
      break;
    case mojom::FileOpenMode::kAlwaysCreate:
      flags |= base::File::FLAG_CREATE_ALWAYS;
      break;
    case mojom::FileOpenMode::kOpenIfExistsAndTruncate:
      flags |= base::File::FLAG_OPEN_TRUNCATED;
      break;
    default:
      NOTREACHED();
      return;
  }

  switch (read_access) {
    case mojom::FileReadAccess::kReadNotAllowed:
      break;
    case mojom::FileReadAccess::kReadAllowed:
      flags |= base::File::FLAG_READ;
      break;
    default:
      NOTREACHED();
      break;
  }

  switch (write_access) {
    case mojom::FileWriteAccess::kWriteNotAllowed:
      break;
    case mojom::FileWriteAccess::kWriteAllowed:
      flags |= base::File::FLAG_WRITE;
      break;
    case mojom::FileWriteAccess::kAppendOnly:
      flags |= base::File::FLAG_APPEND;
      break;
    default:
      NOTREACHED();
      break;
  }

  const base::FilePath full_path = MakeAbsolute(path);
  base::File file(full_path, flags);
  base::File::Error error = base::File::FILE_OK;
  if (!file.IsValid())
    error = file.error_details();
  std::move(callback).Run(error, std::move(file));
}

void FilesystemImpl::RemoveFile(const base::FilePath& path,
                                RemoveFileCallback callback) {
  std::move(callback).Run(
      base::DeleteFile(MakeAbsolute(path), /*recursive=*/false));
}

void FilesystemImpl::CreateDirectory(const base::FilePath& path,
                                     CreateDirectoryCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  base::CreateDirectoryAndGetError(MakeAbsolute(path), &error);
  std::move(callback).Run(error);
}

void FilesystemImpl::RemoveDirectory(const base::FilePath& path,
                                     RemoveDirectoryCallback callback) {
  const base::FilePath full_path = MakeAbsolute(path);
  if (!base::DirectoryExists(full_path)) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(base::DeleteFile(full_path, /*recursive=*/false));
}

void FilesystemImpl::GetFileInfo(const base::FilePath& path,
                                 GetFileInfoCallback callback) {
  base::File::Info info;
  if (base::GetFileInfo(MakeAbsolute(path), &info))
    std::move(callback).Run(std::move(info));
  else
    std::move(callback).Run(base::nullopt);
}

void FilesystemImpl::RenameFile(const base::FilePath& old_path,
                                const base::FilePath& new_path,
                                RenameFileCallback callback) {
  base::File::Error error = base::File::FILE_OK;
  base::ReplaceFile(MakeAbsolute(old_path), MakeAbsolute(new_path), &error);
  std::move(callback).Run(error);
}

void FilesystemImpl::LockFile(const base::FilePath& path,
                              LockFileCallback callback) {
  FileErrorOr<base::File> result = LockFileLocal(MakeAbsolute(path));
  if (result.is_error()) {
    std::move(callback).Run(result.error(), mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::FileLock> lock;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FileLockImpl>(MakeAbsolute(path),
                                     std::move(result.value())),
      lock.InitWithNewPipeAndPassReceiver());
  std::move(callback).Run(base::File::FILE_OK, std::move(lock));
}

// static
FileErrorOr<base::File> FilesystemImpl::LockFileLocal(
    const base::FilePath& path) {
  DCHECK(path.IsAbsolute());
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                            base::File::FLAG_WRITE);
  if (!file.IsValid())
    return file.error_details();

  if (!GetLockTable().AddLock(path))
    return base::File::FILE_ERROR_IN_USE;

#if !defined(OS_FUCHSIA)
  base::File::Error error = file.Lock();
  if (error != base::File::FILE_OK)
    return error;
#endif

  return file;
}

// static
void FilesystemImpl::UnlockFileLocal(const base::FilePath& path) {
  GetLockTable().RemoveLock(path);
}

// static
FileErrorOr<std::vector<base::FilePath>> FilesystemImpl::GetDirectoryEntries(
    const base::FilePath& path,
    mojom::GetEntriesMode mode) {
  DCHECK(path.IsAbsolute());
  int file_types = base::FileEnumerator::FILES;
  if (mode == mojom::GetEntriesMode::kFilesAndDirectories)
    file_types |= base::FileEnumerator::DIRECTORIES;
  base::FileEnumerator enumerator(
      path, /*recursive=*/false, file_types,
      /*pattern=*/base::FilePath::StringType(),
      base::FileEnumerator::FolderSearchPolicy::ALL,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  std::vector<base::FilePath> entries;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    entries.push_back(path);
  }
  if (enumerator.GetError() != base::File::FILE_OK)
    return enumerator.GetError();
  return entries;
}

base::FilePath FilesystemImpl::MakeAbsolute(const base::FilePath& path) const {
  // The DCHECK is a reasonable assertion: this object is only called into via
  // Mojo, and type-map traits for |storage.mojom.StrictRelativePath| ensure
  // that messages can only reach this object if they carry strictly relative
  // paths.
  DCHECK(!path.IsAbsolute());
  return root_.Append(path);
}

}  // namespace storage
