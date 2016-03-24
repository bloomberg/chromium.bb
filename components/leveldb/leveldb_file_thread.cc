// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_file_thread.h"

#include <set>

#include "base/bind.h"
#include "mojo/message_pump/message_pump_mojo.h"
#include "mojo/platform_handle/platform_handle_functions.h"

namespace leveldb {

namespace {
const char kLevelDBFileThreadName[] = "LevelDB_File_Thread";
}  // namespace

struct LevelDBFileThread::OpaqueLock {
  filesystem::FilePtr lock_file;
};

struct LevelDBFileThread::OpaqueDir {
  explicit OpaqueDir(
      mojo::InterfacePtrInfo<filesystem::Directory> directory_info) {
    directory.Bind(std::move(directory_info));
  }

  filesystem::DirectoryPtr directory;
};

struct LevelDBFileThread::WaitableEventDependencies {
  WaitableEventDependencies() {}
  ~WaitableEventDependencies() {}
  std::set<filesystem::DirectoryPtr*> directories;
  std::set<filesystem::FilePtr*> files;
};

LevelDBFileThread::LevelDBFileThread()
    : base::Thread(kLevelDBFileThreadName),
      outstanding_opaque_dirs_(0) {
  base::Thread::Options options;
  options.message_pump_factory =
      base::Bind(&mojo::common::MessagePumpMojo::Create);
  StartWithOptions(options);
}

LevelDBFileThread::OpaqueDir* LevelDBFileThread::RegisterDirectory(
    filesystem::DirectoryPtr directory) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  OpaqueDir* out_dir = nullptr;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::RegisterDirectoryImpl,
                            this,
                            &done_event,
                            base::Passed(directory.PassInterface()),
                            &out_dir));
  done_event.Wait();

  return out_dir;
}

void LevelDBFileThread::UnregisterDirectory(OpaqueDir* dir) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::UnregisterDirectoryImpl,
                            this,
                            &done_event,
                            dir));
  done_event.Wait();
}

base::File LevelDBFileThread::OpenFileHandle(OpaqueDir* dir,
                                             const std::string& name,
                                             uint32_t open_flags) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  base::File file;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::OpenFileHandleImpl, this,
                            dir, name, &done_event, open_flags, &file));
  done_event.Wait();

  return file;
}

filesystem::FileError LevelDBFileThread::SyncDirectory(
    OpaqueDir* dir,
    const std::string& name) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::SyncDirectoryImpl, this,
                            dir, name, &done_event, &error));
  done_event.Wait();

  return error;
}

bool LevelDBFileThread::FileExists(OpaqueDir* dir,
                                   const std::string& name) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  bool exists;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::FileExistsImpl, this,
                            dir, name, &done_event, &exists));
  done_event.Wait();

  return exists;
}

filesystem::FileError LevelDBFileThread::GetChildren(
    OpaqueDir* dir,
    const std::string& path,
    std::vector<std::string>* result) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::GetChildrenImpl, this,
                            dir, path, result, &done_event, &error));
  done_event.Wait();

  return error;
}

filesystem::FileError LevelDBFileThread::Delete(OpaqueDir* dir,
                                                const std::string& path,
                                                uint32_t delete_flags) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&LevelDBFileThread::DeleteImpl, this,
                                     dir, path, delete_flags, &done_event,
                                     &error));
  done_event.Wait();

  return error;
}

filesystem::FileError LevelDBFileThread::CreateDir(OpaqueDir* dir,
                                                   const std::string& path) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::CreateDirImpl, this,
                            dir, path, &done_event, &error));
  done_event.Wait();

  return error;
}

filesystem::FileError LevelDBFileThread::GetFileSize(OpaqueDir* dir,
                                                     const std::string& path,
                                                     uint64_t* file_size) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::GetFileSizeImpl, this,
                            dir, path, file_size, &done_event, &error));
  done_event.Wait();

  return error;
}

filesystem::FileError LevelDBFileThread::RenameFile(
    OpaqueDir* dir,
    const std::string& old_path,
    const std::string& new_path) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::RenameFileImpl, this,
                            dir, old_path, new_path, &done_event, &error));
  done_event.Wait();

  return error;
}

std::pair<filesystem::FileError, LevelDBFileThread::OpaqueLock*>
LevelDBFileThread::LockFile(OpaqueDir* dir,
                            const std::string& path) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  filesystem::FileError error;
  OpaqueLock* out_lock = nullptr;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::LockFileImpl, this,
                            dir, path, &done_event, &error, &out_lock));
  done_event.Wait();

  return std::make_pair(error, out_lock);
}

filesystem::FileError LevelDBFileThread::UnlockFile(OpaqueLock* lock) {
  DCHECK_NE(GetThreadId(), base::PlatformThread::CurrentId());

  // Take ownership of the incoming lock so it gets destroyed whatever happens.
  scoped_ptr<OpaqueLock> scoped_lock(lock);

  filesystem::FileError error;

  // This proxies to the other thread, which proxies to mojo. Only on the reply
  // from mojo do we return from this.
  base::WaitableEvent done_event(false, false);
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&LevelDBFileThread::UnlockFileImpl, this,
                            base::Passed(&scoped_lock), &done_event, &error));
  done_event.Wait();

  return error;
}

LevelDBFileThread::~LevelDBFileThread() {
  Stop();
  DCHECK_EQ(0, outstanding_opaque_dirs_);
}

bool LevelDBFileThread::RegisterDirAndWaitableEvent(
    OpaqueDir* dir,
    base::WaitableEvent* done_event) {
  if (!dir->directory.is_bound()) {
    // The directory went out of scope between the PostTask on the other thread
    // and now.
    done_event->Signal();
    return true;
  }

  waitable_event_dependencies_[done_event].directories.insert(&dir->directory);
  return false;
}

void LevelDBFileThread::CompleteWaitableEvent(base::WaitableEvent* done_event) {
  // Clean up the dependencies that we no longer care about.
  waitable_event_dependencies_.erase(done_event);
  done_event->Signal();
}

void LevelDBFileThread::OnConnectionError() {
  // One of our interface ptrs has become unbound. Signal the event which has
  // it as a dependency.
  auto it = waitable_event_dependencies_.begin();
  while (it != waitable_event_dependencies_.end()) {
    bool unbound_ptr_found = false;
    for (const auto* dir : it->second.directories) {
      if (!dir->is_bound()) {
        unbound_ptr_found = true;
        break;
      }
    }

    if (!unbound_ptr_found) {
      for (const auto* file : it->second.files) {
        if (!file->is_bound()) {
          unbound_ptr_found = true;
          break;
        }
      }
    }

    if (unbound_ptr_found) {
      base::WaitableEvent* e = it->first;
      it = waitable_event_dependencies_.erase(it);
      e->Signal();
    } else {
      ++it;
    }
  }
}

void LevelDBFileThread::OnSimpleComplete(base::WaitableEvent* done_event,
                                         filesystem::FileError* out_error,
                                         filesystem::FileError in_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());
  *out_error = in_error;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::RegisterDirectoryImpl(
    base::WaitableEvent* done_event,
    mojo::InterfacePtrInfo<filesystem::Directory> directory_info,
    OpaqueDir** out_dir) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  // Take the Directory pipe and bind it on this thread.
  *out_dir = new OpaqueDir(std::move(directory_info));
  outstanding_opaque_dirs_++;

  // Register the connection error handler for the resulting DirectoryPtr
  (*out_dir)->directory.set_connection_error_handler(
      base::Bind(&LevelDBFileThread::OnConnectionError, this));

  done_event->Signal();
}

void LevelDBFileThread::UnregisterDirectoryImpl(
    base::WaitableEvent* done_event,
    OpaqueDir* dir) {
  // Only delete the directories on the thread that owns them.
  delete dir;
  outstanding_opaque_dirs_--;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::OpenFileHandleImpl(OpaqueDir* dir,
                                           std::string name,
                                           base::WaitableEvent* done_event,
                                           uint32_t open_flags,
                                           base::File* out_file) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  dir->directory->OpenFileHandle(
      mojo::String::From(name), open_flags,
      base::Bind(&LevelDBFileThread::OnOpenFileHandleComplete, this, done_event,
                 out_file));
}

void LevelDBFileThread::OnOpenFileHandleComplete(
    base::WaitableEvent* done_event,
    base::File* output_file,
    filesystem::FileError err,
    mojo::ScopedHandle handle) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (err != filesystem::FileError::OK) {
    *output_file = base::File(static_cast<base::File::Error>(err));
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

  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::SyncDirectoryImpl(OpaqueDir* dir,
                                          std::string name,
                                          base::WaitableEvent* done_event,
                                          filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  // Step one: open the directory |name| from the toplevel directory.
  scoped_ptr<filesystem::DirectoryPtr> target(new filesystem::DirectoryPtr);

  dir->directory->OpenDirectory(
      name, GetProxy(target.get()),
      filesystem::kFlagRead | filesystem::kFlagWrite,
      base::Bind(&LevelDBFileThread::OnSyncDirctoryOpened, this,
                 base::Passed(&target), done_event, out_error));
}

void LevelDBFileThread::OnSyncDirctoryOpened(
    scoped_ptr<filesystem::DirectoryPtr> dir,
    base::WaitableEvent* done_event,
    filesystem::FileError* out_error,
    filesystem::FileError in_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (in_error != filesystem::FileError::OK) {
    *out_error = in_error;
    CompleteWaitableEvent(done_event);
    return;
  }

  // Add a dependency between the new directory we opened and the current
  // waitable event.
  dir->set_connection_error_handler(
      base::Bind(&LevelDBFileThread::OnConnectionError, this));
  waitable_event_dependencies_[done_event].directories.insert(dir.get());

  // We move the object into the bind before we call. Copy to the stack.
  filesystem::DirectoryPtr* local = dir.get();
  (*local)->Flush(base::Bind(&LevelDBFileThread::OnSyncDirectoryComplete, this,
                             base::Passed(&dir), done_event, out_error));
}

void LevelDBFileThread::OnSyncDirectoryComplete(
    scoped_ptr<filesystem::DirectoryPtr> dir,
    base::WaitableEvent* done_event,
    filesystem::FileError* out_error,
    filesystem::FileError in_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());
  *out_error = in_error;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::FileExistsImpl(OpaqueDir* dir,
                                       std::string name,
                                       base::WaitableEvent* done_event,
                                       bool* exists) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  dir->directory->Exists(
      mojo::String::From(name),
      base::Bind(&LevelDBFileThread::OnFileExistsComplete, this,
                 done_event, exists));
}

void LevelDBFileThread::OnFileExistsComplete(base::WaitableEvent* done_event,
                                             bool* exists,
                                             filesystem::FileError err,
                                             bool in_exists) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());
  *exists = in_exists;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::GetChildrenImpl(OpaqueDir* dir,
                                        std::string name,
                                        std::vector<std::string>* contents,
                                        base::WaitableEvent* done_event,
                                        filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  // Step one: open the directory |name| from the toplevel directory.
  scoped_ptr<filesystem::DirectoryPtr> target(new filesystem::DirectoryPtr);
  mojo::InterfaceRequest<filesystem::Directory> proxy = GetProxy(target.get());
  dir->directory->OpenDirectory(
      name, std::move(proxy), filesystem::kFlagRead | filesystem::kFlagWrite,
      base::Bind(&LevelDBFileThread::OnGetChildrenOpened, this,
                 base::Passed(&target), contents, done_event, out_error));
}

void LevelDBFileThread::OnGetChildrenOpened(
    scoped_ptr<filesystem::DirectoryPtr> dir,
    std::vector<std::string>* contents,
    base::WaitableEvent* done_event,
    filesystem::FileError* out_error,
    filesystem::FileError in_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (in_error != filesystem::FileError::OK) {
    *out_error = in_error;
    CompleteWaitableEvent(done_event);
    return;
  }

  // Add a dependency between the new directory we opened and the current
  // waitable event.
  dir->set_connection_error_handler(
      base::Bind(&LevelDBFileThread::OnConnectionError, this));
  waitable_event_dependencies_[done_event].directories.insert(dir.get());

  // We move the object into the bind before we call. Copy to the stack.
  filesystem::DirectoryPtr* local = dir.get();
  (*local)->Read(base::Bind(&LevelDBFileThread::OnGetChildrenComplete, this,
                            base::Passed(&dir), contents, done_event,
                            out_error));
}

void LevelDBFileThread::OnGetChildrenComplete(
    scoped_ptr<filesystem::DirectoryPtr> dir,
    std::vector<std::string>* out_contents,
    base::WaitableEvent* done_event,
    filesystem::FileError* out_error,
    filesystem::FileError in_error,
    mojo::Array<filesystem::DirectoryEntryPtr> directory_contents) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (!directory_contents.is_null()) {
    for (size_t i = 0; i < directory_contents.size(); ++i)
      out_contents->push_back(directory_contents[i]->name.To<std::string>());
  }

  *out_error = in_error;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::DeleteImpl(OpaqueDir* dir,
                                   std::string name,
                                   uint32_t delete_flags,
                                   base::WaitableEvent* done_event,
                                   filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  dir->directory->Delete(mojo::String::From(name), delete_flags,
                         base::Bind(&LevelDBFileThread::OnSimpleComplete, this,
                                    done_event, out_error));
}

void LevelDBFileThread::CreateDirImpl(OpaqueDir* dir,
                                      std::string name,
                                      base::WaitableEvent* done_event,
                                      filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  dir->directory->OpenDirectory(
      name, nullptr,
      filesystem::kFlagRead | filesystem::kFlagWrite | filesystem::kFlagCreate,
      base::Bind(&LevelDBFileThread::OnSimpleComplete, this, done_event,
                 out_error));
}

void LevelDBFileThread::GetFileSizeImpl(OpaqueDir* dir,
                                        const std::string& path,
                                        uint64_t* file_size,
                                        base::WaitableEvent* done_event,
                                        filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  dir->directory->StatFile(
      path, base::Bind(&LevelDBFileThread::OnGetFileSizeImpl, this, file_size,
                       done_event, out_error));
}

void LevelDBFileThread::OnGetFileSizeImpl(
    uint64_t* file_size,
    base::WaitableEvent* done_event,
    filesystem::FileError* out_error,
    filesystem::FileError in_error,
    filesystem::FileInformationPtr file_info) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());
  if (file_info)
    *file_size = file_info->size;
  *out_error = in_error;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::RenameFileImpl(OpaqueDir* dir,
                                       const std::string& old_path,
                                       const std::string& new_path,
                                       base::WaitableEvent* done_event,
                                       filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  dir->directory->Rename(mojo::String::From(old_path),
                         mojo::String::From(new_path),
                         base::Bind(&LevelDBFileThread::OnSimpleComplete, this,
                                    done_event, out_error));
}

void LevelDBFileThread::LockFileImpl(OpaqueDir* dir,
                                     const std::string& path,
                                     base::WaitableEvent* done_event,
                                     filesystem::FileError* out_error,
                                     OpaqueLock** out_lock) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (RegisterDirAndWaitableEvent(dir, done_event))
    return;

  // Since a lock is associated with a file descriptor, we need to open and
  // have a persistent file on the other side of the connection.
  scoped_ptr<filesystem::FilePtr> target(new filesystem::FilePtr);
  mojo::InterfaceRequest<filesystem::File> proxy = GetProxy(target.get());
  dir->directory->OpenFile(
      mojo::String::From(path), std::move(proxy),
      filesystem::kFlagOpenAlways | filesystem::kFlagRead |
          filesystem::kFlagWrite,
      base::Bind(&LevelDBFileThread::OnOpenLockFileComplete, this,
                 base::Passed(&target), done_event, out_error, out_lock));
}

void LevelDBFileThread::OnOpenLockFileComplete(
    scoped_ptr<filesystem::FilePtr> file,
    base::WaitableEvent* done_event,
    filesystem::FileError* out_error,
    OpaqueLock** out_lock,
    filesystem::FileError in_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  if (in_error != filesystem::FileError::OK) {
    *out_error = in_error;
    CompleteWaitableEvent(done_event);
    return;
  }

  // Add a dependency between the new file we opened and the current waitable
  // event. (This dependency will get cleared in OnLockFileComplete if we
  // complete this call safely.)
  file->set_connection_error_handler(
      base::Bind(&LevelDBFileThread::OnConnectionError, this));
  waitable_event_dependencies_[done_event].files.insert(file.get());

  filesystem::FilePtr* local = file.get();
  (*local)->Lock(base::Bind(&LevelDBFileThread::OnLockFileComplete, this,
                            base::Passed(&file), done_event, out_error,
                            out_lock));
}

void LevelDBFileThread::OnLockFileComplete(scoped_ptr<filesystem::FilePtr> file,
                                           base::WaitableEvent* done_event,
                                           filesystem::FileError* out_error,
                                           OpaqueLock** out_lock,
                                           filesystem::FileError in_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  *out_error = in_error;

  if (in_error == filesystem::FileError::OK) {
    OpaqueLock* l = new OpaqueLock;
    l->lock_file = std::move(*file);
    *out_lock = l;
  }

  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::UnlockFileImpl(scoped_ptr<OpaqueLock> lock,
                                       base::WaitableEvent* done_event,
                                       filesystem::FileError* out_error) {
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());

  OpaqueLock* local = lock.get();
  local->lock_file->Unlock(base::Bind(&LevelDBFileThread::OnUnlockFileCompleted,
                                      this, base::Passed(&lock), done_event,
                                      out_error));
}

void LevelDBFileThread::OnUnlockFileCompleted(scoped_ptr<OpaqueLock> lock,
                                              base::WaitableEvent* done_event,
                                              filesystem::FileError* out_error,
                                              filesystem::FileError in_error) {
  // We're passed the OpauqeLock here for ownership reasons, but it's going to
  // get destructed on its own here.
  DCHECK_EQ(GetThreadId(), base::PlatformThread::CurrentId());
  *out_error = in_error;
  CompleteWaitableEvent(done_event);
}

void LevelDBFileThread::Init() {
}

void LevelDBFileThread::CleanUp() {
}

}  // namespace leveldb
