// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_system/file_system_dispatcher.h"

#include "base/file_util.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

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
    IPC_MESSAGE_HANDLER(ViewMsg_OpenFileSystemRequest_Complete,
                        OnOpenFileSystemRequestComplete)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidSucceed, DidSucceed)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidReadDirectory, DidReadDirectory)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidReadMetadata, DidReadMetadata)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidFail, DidFail)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidWrite, DidWrite)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FileSystemDispatcher::OpenFileSystem(
    const GURL& origin_url, fileapi::FileSystemType type,
    long long size, fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  ChildThread::current()->Send(new ViewHostMsg_OpenFileSystemRequest(
      request_id, origin_url, type, size));
}

bool FileSystemDispatcher::Move(
    const FilePath& src_path,
    const FilePath& dest_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(new ViewHostMsg_FileSystem_Move(
      request_id, src_path, dest_path));
}

bool FileSystemDispatcher::Copy(
    const FilePath& src_path,
    const FilePath& dest_path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(new ViewHostMsg_FileSystem_Copy(
      request_id, src_path, dest_path));
}

bool FileSystemDispatcher::Remove(
    const FilePath& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Remove(request_id, path));
}

bool FileSystemDispatcher::ReadMetadata(
    const FilePath& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_ReadMetadata(request_id, path));
}

bool FileSystemDispatcher::Create(
    const FilePath& path,
    bool exclusive,
    bool is_directory,
    bool recursive,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(new ViewHostMsg_FileSystem_Create(
      request_id, path, exclusive, is_directory, recursive));
}

bool FileSystemDispatcher::Exists(
    const FilePath& path,
    bool is_directory,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Exists(request_id, path, is_directory));
}

bool FileSystemDispatcher::ReadDirectory(
    const FilePath& path,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_ReadDirectory(request_id, path));
}

bool FileSystemDispatcher::Truncate(
    const FilePath& path,
    int64 offset,
    int* request_id_out,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Truncate(request_id, path, offset))) {
    if (request_id_out)
      *request_id_out = request_id;
    return true;
  }
  return false;
}

bool FileSystemDispatcher::Write(
    const FilePath& path,
    const GURL& blob_url,
    int64 offset,
    int* request_id_out,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  if (ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Write(
          request_id, path, blob_url, offset))) {
    if (request_id_out)
      *request_id_out = request_id;
    return true;
  }
  return false;
}

bool FileSystemDispatcher::Cancel(
    int request_id_to_cancel,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_CancelWrite(request_id, request_id_to_cancel));
}

bool FileSystemDispatcher::TouchFile(
    const FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    fileapi::FileSystemCallbackDispatcher* dispatcher) {
  int request_id = dispatchers_.Add(dispatcher);
  return ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_TouchFile(
          request_id, path, last_access_time, last_modified_time));
}

void FileSystemDispatcher::OnOpenFileSystemRequestComplete(
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

void FileSystemDispatcher::DidSucceed(int request_id) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidSucceed();
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::DidReadMetadata(
    int request_id, const base::PlatformFileInfo& file_info) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidReadMetadata(file_info);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::DidReadDirectory(
    int request_id,
    const std::vector<base::file_util_proxy::Entry>& entries,
    bool has_more) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidReadDirectory(entries, has_more);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::DidFail(
    int request_id, base::PlatformFileError error_code) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  dispatcher->DidFail(error_code);
  dispatchers_.Remove(request_id);
}

void FileSystemDispatcher::DidWrite(
    int request_id, int64 bytes, bool complete) {
  fileapi::FileSystemCallbackDispatcher* dispatcher =
      dispatchers_.Lookup(request_id);
  DCHECK(dispatcher);
  // TODO(ericu): Coming soon.
  // dispatcher->DidWrite(bytes, complete);
  if (complete)
    dispatchers_.Remove(request_id);
}
