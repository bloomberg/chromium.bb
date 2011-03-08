// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/file_system/file_system_dispatcher.h"

#include "base/file_util.h"
#include "content/common/child_thread.h"
#include "content/common/file_system_messages.h"

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
    IPC_MESSAGE_HANDLER(FileSystemMsg_OpenComplete, OnOpenComplete)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidSucceed, OnDidSucceed)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidReadDirectory, OnDidReadDirectory)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidReadMetadata, OnDidReadMetadata)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidFail, OnDidFail)
    IPC_MESSAGE_HANDLER(FileSystemMsg_DidWrite, OnDidWrite)
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

bool FileSystemDispatcher::Move(
    const FilePath& src_path,
    const FilePath& dest_path,
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
    const FilePath& src_path,
    const FilePath& dest_path,
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
    const FilePath& path,
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
    const FilePath& path,
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
    const FilePath& path,
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
    const FilePath& path,
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
    const FilePath& path,
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
    const FilePath& path,
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
    const FilePath& path,
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
    const FilePath& path,
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

void FileSystemDispatcher::OnOpenComplete(
    int request_id, bool accepted, const std::string& name,
    const FilePath& root_path) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  if (accepted)
    dispatcher->DidOpenFileSystem(name, root_path);
  else
    dispatcher->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
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
    int request_id, const base::PlatformFileInfo& file_info) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidReadMetadata(file_info);
  dispatchers_.Remove(request_id);
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
