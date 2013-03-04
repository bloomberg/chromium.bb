// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fileapi/file_system_dispatcher.h"

#include "base/file_util.h"
#include "base/process.h"
#include "content/common/child_thread.h"
#include "content/common/fileapi/file_system_messages.h"

namespace content {

FileSystemDispatcher::FileSystemDispatcher() {
}

FileSystemDispatcher::~FileSystemDispatcher() {
  // Make sure we fire all the remaining callbacks.
  for (IDMap<fileapi::FileSystemCallbackDispatcher, IDMapOwnPointer>::iterator
           iter(&dispatchers_); !iter.IsAtEnd(); iter.Advance()) {
    int request_id = iter.GetCurrentKey();
    fileapi::FileSystemCallbackDispatcher* dispatcher = iter.GetCurrentValue();
    DCHECK(dispatcher);
    dispatcher->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    dispatchers_.Remove(request_id);
  }
}

bool FileSystemDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FileSystemDispatcher, msg)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidOpenFileSystem, OnDidOpenFileSystem)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidSucceed, OnDidSucceed)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidReadDirectory, OnDidReadDirectory)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidReadMetadata, OnDidReadMetadata)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidCreateSnapshotFile,
                        OnDidCreateSnapshotFile)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidFail, OnDidFail)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidWrite, OnDidWrite)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidOpenFile, OnDidOpenFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool FileSystemDispatcher::OpenFileSystem(
    const GURL& origin_url, fileapi::FileSystemType type,
    long long size, bool create,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(new FileSystemHostMsg_Open(
          request_id, origin_url, type, size, create))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::DeleteFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(new FileSystemHostMsg_DeleteFileSystem(
          request_id, origin_url, type))) {
    dispatchers_.Remove(request_id);
    return false;
  }
  return true;
}

bool FileSystemDispatcher::Move(
    const GURL& src_path,
    const GURL& dest_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(new FileSystemHostMsg_Move(
          request_id, src_path, dest_path))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::Copy(
    const GURL& src_path,
    const GURL& dest_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(new FileSystemHostMsg_Copy(
          request_id, src_path, dest_path))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::Remove(
    const GURL& path,
    bool recursive,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemMsg_Remove(request_id, path, recursive))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::ReadMetadata(
    const GURL& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_ReadMetadata(request_id, path))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::Create(
    const GURL& path,
    bool exclusive,
    bool is_directory,
    bool recursive,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(new FileSystemHostMsg_Create(
          request_id, path, exclusive, is_directory, recursive))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::Exists(
    const GURL& path,
    bool is_directory,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_Exists(request_id, path, is_directory))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::ReadDirectory(
    const GURL& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_ReadDirectory(request_id, path))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::Truncate(
    const GURL& path,
    int64 offset,
    int* request_id_out,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_Truncate(request_id, path, offset))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  if (request_id_out)
    *request_id_out = request_id;
  return true;
}

bool FileSystemDispatcher::Write(
    const GURL& path,
    const GURL& blob_url,
    int64 offset,
    int* request_id_out,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_Write(request_id, path, blob_url, offset))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  if (request_id_out)
    *request_id_out = request_id;
  return true;
}

bool FileSystemDispatcher::Cancel(
    int request_id_to_cancel,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(new FileSystemHostMsg_CancelWrite(
          request_id, request_id_to_cancel))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::TouchFile(
    const GURL& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_TouchFile(
              request_id, path, last_access_time, last_modified_time))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::OpenFile(
    const GURL& file_path,
    int file_flags,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_OpenFile(
              request_id, file_path, file_flags))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }

  return true;
}

bool FileSystemDispatcher::NotifyCloseFile(const GURL& file_path) {
  return ChildThread::current()->Send(
      new FileSystemHostMsg_NotifyCloseFile(file_path));
}

bool FileSystemDispatcher::CreateSnapshotFile(
    const GURL& file_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_CreateSnapshotFile(
              request_id, file_path))) {
    dispatchers_.Remove(request_id);  // destroys |dispatcher|
    return false;
  }
  return true;
}

bool FileSystemDispatcher::CreateSnapshotFile_Deprecated(
    const GURL& blob_url,
    const GURL& file_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (!ChildThread::current()->Send(
          new FileSystemHostMsg_CreateSnapshotFile_Deprecated(
              request_id, blob_url, file_path))) {
    dispatchers_.Remove(request_id); // destroys |dispatcher|
    return false;
  }
  return true;
}

void FileSystemDispatcher::OnDidOpenFileSystem(int request_id,
                                               const std::string& name,
                                               const GURL& root) {
  DCHECK(root.is_valid());
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidOpenFileSystem(name, root);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::OnDidSucceed(int request_id) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidSucceed();
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::OnDidReadMetadata(
    int request_id, const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidReadMetadata(file_info, platform_path);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::OnDidCreateSnapshotFile(
    int request_id, const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidCreateSnapshotFile(file_info, platform_path);
  dispatchers_.Remove(request_id);
  ChildThread::current()->Send(
      new FileSystemHostMsg_DidReceiveSnapshotFile(request_id));
}

void FileSystemDispatcher::OnDidReadDirectory(
    int request_id,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidReadDirectory(entries, has_more);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::OnDidFail(
    int request_id, base::PlatformFileError error_code) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidFail(error_code);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::OnDidWrite(
    int request_id, int64 bytes, bool complete) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidWrite(bytes, complete);
  if (complete)
    dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::OnDidOpenFile(
    int request_id, IPC::PlatformFileForTransit file) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidOpenFile(IPC::PlatformFileForTransitToPlatformFile(file));
  dispatchers_.Remove(request_id);
}

}  // namespace content
