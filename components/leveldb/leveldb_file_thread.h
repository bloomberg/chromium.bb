// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_LEVELDB_FILE_THREAD_H_
#define COMPONENTS_LEVELDB_LEVELDB_FILE_THREAD_H_

#include <string>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"

namespace leveldb {

// The thread which services file requests from leveldb.
//
// MojoEnv is an object passed to the leveldb implementation which can be
// called from multiple threads. mojo pipes are bound to a single
// thread. Because of this mismatch, we create a thread which owns the mojo
// pipe, sends and receives messages.
//
// All public methods can be accessed from any thread.
class LevelDBFileThread : public base::Thread,
                          public base::RefCountedThreadSafe<LevelDBFileThread> {
 public:
  LevelDBFileThread();

  // A private struct to hide the underlying file that holds the lock from our
  // callers, forcing them to go through our LockFile()/UnlockFile() interface
  // so that they don't try to use the underlying pointer from an unsafe thread.
  struct OpaqueLock;

  // A private struct to hide the underlying root directory that we're
  // operating in. LevelDBFileThread will want to own all the directory
  // pointers, so while opening a database, we pass the directory to the thread
  // it will be operated on.
  struct OpaqueDir;

  // Passes ownership of a |directory| to the other thread, giving a reference
  // handle back to the caller.
  OpaqueDir* RegisterDirectory(filesystem::DirectoryPtr directory);
  void UnregisterDirectory(OpaqueDir* dir);

  // Synchronously calls Directory.OpenFileHandle().
  base::File OpenFileHandle(OpaqueDir* dir,
                            const std::string& name,
                            uint32_t open_flags);

  // Synchronously flushes |directory_|.
  filesystem::FileError SyncDirectory(OpaqueDir* dir,
                                      const std::string& name);

  // Synchronously checks whether |name| exists.
  bool FileExists(OpaqueDir* dir,
                  const std::string& name);

  // Synchronously returns the filenames of all files in |path|.
  filesystem::FileError GetChildren(OpaqueDir* dir,
                                    const std::string& path,
                                    std::vector<std::string>* result);

  // Synchronously deletes |path|.
  filesystem::FileError Delete(OpaqueDir* dir,
                               const std::string& path,
                               uint32_t delete_flags);

  // Synchronously creates |path|.
  filesystem::FileError CreateDir(OpaqueDir* dir,
                               const std::string& path);

  // Synchronously gets the size of a file.
  filesystem::FileError GetFileSize(OpaqueDir* dir,
                                    const std::string& path,
                                    uint64_t* file_size);

  // Synchronously renames a file.
  filesystem::FileError RenameFile(OpaqueDir* dir,
                                   const std::string& old_path,
                                   const std::string& new_path);

  // Synchronously locks a file. Returns both the file return code, and if OK,
  // an opaque object to the lock to enforce going through this interface to
  // unlock the file so that unlocking happens on the correct thread.
  std::pair<filesystem::FileError, OpaqueLock*> LockFile(
      OpaqueDir* dir, const std::string& path);

  // Unlocks a file. LevelDBFileThread takes ownership of lock. (We don't make
  // this a scoped_ptr because exporting the ctor/dtor for this struct publicly
  // defeats the purpose of the struct.)
  filesystem::FileError UnlockFile(OpaqueLock* lock);

 private:
  friend class base::RefCountedThreadSafe<LevelDBFileThread>;
  ~LevelDBFileThread() override;

  void RegisterDirectoryImpl(
      base::WaitableEvent* done_event,
      mojo::InterfacePtrInfo<filesystem::Directory> directory_info,
      OpaqueDir** out_dir);
  void UnregisterDirectoryImpl(base::WaitableEvent* done_event,
                               OpaqueDir* dir);

  // Shared callback function which signals for the methods that only return an
  // error code.
  void OnSimpleComplete(base::WaitableEvent* done_event,
                        filesystem::FileError* out_error,
                        filesystem::FileError in_error);

  void OpenFileHandleImpl(OpaqueDir* dir,
                          std::string name,
                          base::WaitableEvent* done_event,
                          uint32_t open_flags,
                          base::File* out_file);
  void OnOpenFileHandleComplete(base::WaitableEvent* done_event,
                                base::File* output_file,
                                filesystem::FileError err,
                                mojo::ScopedHandle handle);

  void SyncDirectoryImpl(OpaqueDir* dir,
                         std::string name,
                         base::WaitableEvent* done_event,
                         filesystem::FileError* out_error);
  void OnSyncDirctoryOpened(scoped_ptr<filesystem::DirectoryPtr> dir,
                            base::WaitableEvent* done_event,
                            filesystem::FileError* out_error,
                            filesystem::FileError in_error);
  void OnSyncDirectoryComplete(scoped_ptr<filesystem::DirectoryPtr> dir,
                               base::WaitableEvent* done_event,
                               filesystem::FileError* out_error,
                               filesystem::FileError in_error);

  void FileExistsImpl(OpaqueDir* dir,
                      std::string name,
                      base::WaitableEvent* done_event,
                      bool* exists);
  void OnFileExistsComplete(base::WaitableEvent* done_event,
                            bool* exists,
                            filesystem::FileError err,
                            bool in_exists);

  void GetChildrenImpl(OpaqueDir* dir,
                       std::string name,
                       std::vector<std::string>* contents,
                       base::WaitableEvent* done_event,
                       filesystem::FileError* out_error);
  void OnGetChildrenOpened(scoped_ptr<filesystem::DirectoryPtr> dir,
                           std::vector<std::string>* contents,
                           base::WaitableEvent* done_event,
                           filesystem::FileError* out_error,
                           filesystem::FileError in_error);
  void OnGetChildrenComplete(
      scoped_ptr<filesystem::DirectoryPtr> dir,
      std::vector<std::string>* out_contents,
      base::WaitableEvent* done_event,
      filesystem::FileError* out_error,
      filesystem::FileError in_error,
      mojo::Array<filesystem::DirectoryEntryPtr> directory_contents);

  void DeleteImpl(OpaqueDir* dir,
                  std::string name,
                  uint32_t delete_flags,
                  base::WaitableEvent* done_event,
                  filesystem::FileError* out_error);

  void CreateDirImpl(OpaqueDir* dir,
                     std::string name,
                     base::WaitableEvent* done_event,
                     filesystem::FileError* out_error);

  void GetFileSizeImpl(OpaqueDir* dir,
                       const std::string& path,
                       uint64_t* file_size,
                       base::WaitableEvent* done_event,
                       filesystem::FileError* out_error);
  void OnGetFileSizeImpl(uint64_t* file_size,
                         base::WaitableEvent* done_event,
                         filesystem::FileError* out_error,
                         filesystem::FileError in_error,
                         filesystem::FileInformationPtr file_info);

  void RenameFileImpl(OpaqueDir* dir,
                      const std::string& old_path,
                      const std::string& new_path,
                      base::WaitableEvent* done_event,
                      filesystem::FileError* out_error);

  void LockFileImpl(OpaqueDir* dir,
                    const std::string& path,
                    base::WaitableEvent* done_event,
                    filesystem::FileError* out_error,
                    OpaqueLock** out_lock);
  void OnOpenLockFileComplete(scoped_ptr<filesystem::FilePtr> file,
                              base::WaitableEvent* done_event,
                              filesystem::FileError* out_error,
                              OpaqueLock** out_lock,
                              filesystem::FileError in_error);
  void OnLockFileComplete(scoped_ptr<filesystem::FilePtr> file,
                          base::WaitableEvent* done_event,
                          filesystem::FileError* out_error,
                          OpaqueLock** out_lock,
                          filesystem::FileError in_error);

  void UnlockFileImpl(scoped_ptr<OpaqueLock> lock,
                      base::WaitableEvent* done_event,
                      filesystem::FileError* out_error);
  void OnUnlockFileCompleted(scoped_ptr<OpaqueLock> lock,
                             base::WaitableEvent* done_event,
                             filesystem::FileError* out_error,
                             filesystem::FileError in_error);

  // Overridden from base::Thread:
  void Init() override;
  void CleanUp() override;

  int outstanding_opaque_dirs_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBFileThread);
};

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_LEVELDB_FILE_THREAD_H_
