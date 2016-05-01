// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_mojo_proxy.h"

#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "mojo/platform_handle/platform_handle_functions.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace leveldb {

struct LevelDBMojoProxy::OpaqueLock {
  filesystem::FilePtr lock_file;
};

struct LevelDBMojoProxy::OpaqueDir {
  explicit OpaqueDir(
      mojo::InterfacePtrInfo<filesystem::Directory> directory_info) {
    directory.Bind(std::move(directory_info));
  }

  filesystem::DirectoryPtr directory;
};

LevelDBMojoProxy::LevelDBMojoProxy(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)), outstanding_opaque_dirs_(0) {}

LevelDBMojoProxy::OpaqueDir* LevelDBMojoProxy::RegisterDirectory(
    filesystem::DirectoryPtr directory) {
  OpaqueDir* out_dir = nullptr;
  RunInternal(base::Bind(&LevelDBMojoProxy::RegisterDirectoryImpl, this,
                         base::Passed(directory.PassInterface()),
                         &out_dir));

  return out_dir;
}

void LevelDBMojoProxy::UnregisterDirectory(OpaqueDir* dir) {
  RunInternal(base::Bind(&LevelDBMojoProxy::UnregisterDirectoryImpl,
                         this, dir));
}

base::File LevelDBMojoProxy::OpenFileHandle(OpaqueDir* dir,
                                            const std::string& name,
                                            uint32_t open_flags) {
  base::File file;
  RunInternal(base::Bind(&LevelDBMojoProxy::OpenFileHandleImpl, this, dir,
                         name, open_flags, &file));
  return file;
}

filesystem::FileError LevelDBMojoProxy::SyncDirectory(OpaqueDir* dir,
                                                      const std::string& name) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::SyncDirectoryImpl, this, dir,
                         name, &error));
  return error;
}

bool LevelDBMojoProxy::FileExists(OpaqueDir* dir, const std::string& name) {
  bool exists = false;
  RunInternal(base::Bind(&LevelDBMojoProxy::FileExistsImpl, this, dir,
                         name, &exists));
  return exists;
}

filesystem::FileError LevelDBMojoProxy::GetChildren(
    OpaqueDir* dir,
    const std::string& path,
    std::vector<std::string>* result) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::GetChildrenImpl, this, dir,
                         path, result, &error));
  return error;
}

filesystem::FileError LevelDBMojoProxy::Delete(OpaqueDir* dir,
                                               const std::string& path,
                                               uint32_t delete_flags) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::DeleteImpl, this, dir, path,
                         delete_flags, &error));
  return error;
}

filesystem::FileError LevelDBMojoProxy::CreateDir(OpaqueDir* dir,
                                                  const std::string& path) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::CreateDirImpl, this, dir, path,
                         &error));
  return error;
}

filesystem::FileError LevelDBMojoProxy::GetFileSize(OpaqueDir* dir,
                                                    const std::string& path,
                                                    uint64_t* file_size) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::GetFileSizeImpl, this, dir,
                         path, file_size, &error));
  return error;
}

filesystem::FileError LevelDBMojoProxy::RenameFile(
    OpaqueDir* dir,
    const std::string& old_path,
    const std::string& new_path) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::RenameFileImpl, this, dir,
                         old_path, new_path, &error));
  return error;
}

std::pair<filesystem::FileError, LevelDBMojoProxy::OpaqueLock*>
LevelDBMojoProxy::LockFile(OpaqueDir* dir, const std::string& path) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  OpaqueLock* out_lock = nullptr;
  RunInternal(base::Bind(&LevelDBMojoProxy::LockFileImpl, this, dir, path,
                         &error, &out_lock));
  return std::make_pair(error, out_lock);
}

filesystem::FileError LevelDBMojoProxy::UnlockFile(OpaqueLock* lock) {
  // Take ownership of the incoming lock so it gets destroyed whatever happens.
  std::unique_ptr<OpaqueLock> scoped_lock(lock);
  filesystem::FileError error = filesystem::FileError::FAILED;
  RunInternal(base::Bind(&LevelDBMojoProxy::UnlockFileImpl, this,
                         base::Passed(&scoped_lock), &error));
  return error;
}

LevelDBMojoProxy::~LevelDBMojoProxy() {
  DCHECK_EQ(0, outstanding_opaque_dirs_);
}

void LevelDBMojoProxy::RunInternal(const base::Closure& task) {
  if (task_runner_->BelongsToCurrentThread()) {
    task.Run();
  } else {
    base::WaitableEvent done_event(false, false);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LevelDBMojoProxy::DoOnOtherThread,
                   this,
                   task,
                   base::Unretained(&done_event)));
    done_event.Wait();
  }
}

void LevelDBMojoProxy::DoOnOtherThread(const base::Closure& c,
                                       base::WaitableEvent* event) {
  c.Run();
  event->Signal();
}

void LevelDBMojoProxy::RegisterDirectoryImpl(
    mojo::InterfacePtrInfo<filesystem::Directory> directory_info,
    OpaqueDir** out_dir) {
  // Take the Directory pipe and bind it on this thread.
  *out_dir = new OpaqueDir(std::move(directory_info));
  outstanding_opaque_dirs_++;
}

void LevelDBMojoProxy::UnregisterDirectoryImpl(
    OpaqueDir* dir) {
  // Only delete the directories on the thread that owns them.
  delete dir;
  outstanding_opaque_dirs_--;
}

void LevelDBMojoProxy::OpenFileHandleImpl(OpaqueDir* dir,
                                          std::string name,
                                          uint32_t open_flags,
                                          base::File* output_file) {
  mojo::ScopedHandle handle;
  filesystem::FileError error = filesystem::FileError::FAILED;
  bool completed = dir->directory->OpenFileHandle(mojo::String::From(name),
                                                  open_flags, &error, &handle);
  DCHECK(completed);

  if (error != filesystem::FileError::OK) {
    *output_file = base::File(static_cast<base::File::Error>(error));
  } else {
    MojoPlatformHandle platform_handle;
    MojoResult extract_result =
        MojoExtractPlatformHandle(handle.release().value(), &platform_handle);

    if (extract_result == MOJO_RESULT_OK) {
      *output_file = base::File(platform_handle);
    } else {
      NOTREACHED();
      *output_file = base::File(base::File::Error::FILE_ERROR_FAILED);
    }
  }
}

void LevelDBMojoProxy::SyncDirectoryImpl(OpaqueDir* dir,
                                         std::string name,
                                         filesystem::FileError* out_error) {
  filesystem::DirectoryPtr target;
  bool completed = dir->directory->OpenDirectory(
      name, GetProxy(&target), filesystem::kFlagRead | filesystem::kFlagWrite,
      out_error);
  DCHECK(completed);

  if (*out_error != filesystem::FileError::OK)
    return;

  completed = target->Flush(out_error);
  DCHECK(completed);
}

void LevelDBMojoProxy::FileExistsImpl(OpaqueDir* dir,
                                      std::string name,
                                      bool* exists) {
  filesystem::FileError error = filesystem::FileError::FAILED;
  bool completed =
      dir->directory->Exists(mojo::String::From(name), &error, exists);
  DCHECK(completed);
}

void LevelDBMojoProxy::GetChildrenImpl(OpaqueDir* dir,
                                       std::string name,
                                       std::vector<std::string>* out_contents,
                                       filesystem::FileError* out_error) {
  filesystem::DirectoryPtr target;
  filesystem::DirectoryRequest proxy = GetProxy(&target);
  bool completed = dir->directory->OpenDirectory(
      name, std::move(proxy), filesystem::kFlagRead | filesystem::kFlagWrite,
      out_error);
  DCHECK(completed);

  if (*out_error != filesystem::FileError::OK)
    return;

  mojo::Array<filesystem::DirectoryEntryPtr> directory_contents;
  completed = target->Read(out_error, &directory_contents);
  DCHECK(completed);

  if (!directory_contents.is_null()) {
    for (size_t i = 0; i < directory_contents.size(); ++i)
      out_contents->push_back(directory_contents[i]->name.To<std::string>());
  }
}

void LevelDBMojoProxy::DeleteImpl(OpaqueDir* dir,
                                  std::string name,
                                  uint32_t delete_flags,
                                  filesystem::FileError* out_error) {
  bool completed =
      dir->directory->Delete(mojo::String::From(name), delete_flags, out_error);
  DCHECK(completed);
}

void LevelDBMojoProxy::CreateDirImpl(OpaqueDir* dir,
                                     std::string name,
                                     filesystem::FileError* out_error) {
  bool completed = dir->directory->OpenDirectory(
      name, nullptr,
      filesystem::kFlagRead | filesystem::kFlagWrite | filesystem::kFlagCreate,
      out_error);
  DCHECK(completed);
}

void LevelDBMojoProxy::GetFileSizeImpl(OpaqueDir* dir,
                                       const std::string& path,
                                       uint64_t* file_size,
                                       filesystem::FileError* out_error) {
  filesystem::FileInformationPtr info;
  bool completed = dir->directory->StatFile(path, out_error, &info);
  DCHECK(completed);
  if (info)
    *file_size = info->size;
}

void LevelDBMojoProxy::RenameFileImpl(OpaqueDir* dir,
                                      const std::string& old_path,
                                      const std::string& new_path,
                                      filesystem::FileError* out_error) {
  bool completed = dir->directory->Rename(
      mojo::String::From(old_path), mojo::String::From(new_path), out_error);
  DCHECK(completed);
}

void LevelDBMojoProxy::LockFileImpl(OpaqueDir* dir,
                                    const std::string& path,
                                    filesystem::FileError* out_error,
                                    OpaqueLock** out_lock) {
  // Since a lock is associated with a file descriptor, we need to open and
  // have a persistent file on the other side of the connection.
  filesystem::FilePtr target;
  filesystem::FileRequest proxy = GetProxy(&target);
  bool completed = dir->directory->OpenFile(
      mojo::String::From(path), std::move(proxy),
      filesystem::kFlagOpenAlways | filesystem::kFlagRead |
          filesystem::kFlagWrite,
      out_error);
  DCHECK(completed);

  if (*out_error != filesystem::FileError::OK)
    return;

  completed = target->Lock(out_error);
  DCHECK(completed);

  if (*out_error == filesystem::FileError::OK) {
    OpaqueLock* l = new OpaqueLock;
    l->lock_file = std::move(target);
    *out_lock = l;
  }
}

void LevelDBMojoProxy::UnlockFileImpl(std::unique_ptr<OpaqueLock> lock,
                                      filesystem::FileError* out_error) {
  lock->lock_file->Unlock(out_error);
}

}  // namespace leveldb
