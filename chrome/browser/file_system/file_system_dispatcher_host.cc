// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_dispatcher_host.h"

#include "base/file_path.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/file_system/browser_file_system_callback_dispatcher.h"
#include "chrome/browser/file_system/file_system_host_context.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "googleurl/src/gurl.h"

// A class to hold an ongoing openFileSystem completion task.
struct OpenFileSystemCompletionTask {
 public:
  static void Run(
      int request_id,
      const std::string& name,
      const FilePath& root_path,
      FileSystemDispatcherHost* dispatcher_host) {
    // The task is self-destructed.
    new OpenFileSystemCompletionTask(request_id, name, root_path,
        dispatcher_host);
  }

  void DidFinish(base::PlatformFileError error) {
    if (error == base::PLATFORM_FILE_OK)
      dispatcher_host_->Send(
          new ViewMsg_OpenFileSystemRequest_Complete(
              request_id_, true, name_, root_path_));
    else
      dispatcher_host_->Send(
          new ViewMsg_OpenFileSystemRequest_Complete(
              request_id_, false, std::string(), FilePath()));
    delete this;
  }

 private:
  OpenFileSystemCompletionTask(
      int request_id,
      const std::string& name,
      const FilePath& root_path,
      FileSystemDispatcherHost* dispatcher_host)
    : request_id_(request_id),
      name_(name),
      root_path_(root_path),
      dispatcher_host_(dispatcher_host),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    base::FileUtilProxy::CreateDirectory(
        ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE),
        root_path_, false, true, callback_factory_.NewCallback(
            &OpenFileSystemCompletionTask::DidFinish));
  }

  int request_id_;
  std::string name_;
  FilePath root_path_;
  scoped_refptr<FileSystemDispatcherHost> dispatcher_host_;
  base::ScopedCallbackFactory<OpenFileSystemCompletionTask> callback_factory_;
};

FileSystemDispatcherHost::FileSystemDispatcherHost(
    IPC::Message::Sender* sender,
    FileSystemHostContext* file_system_host_context,
    HostContentSettingsMap* host_content_settings_map)
    : message_sender_(sender),
      process_handle_(0),
      shutdown_(false),
      context_(file_system_host_context),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(message_sender_);
}

FileSystemDispatcherHost::~FileSystemDispatcherHost() {
}

void FileSystemDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(!shutdown_);
  DCHECK(!process_handle_);
  DCHECK(process_handle);
  process_handle_ = process_handle;
}

void FileSystemDispatcherHost::Shutdown() {
  message_sender_ = NULL;
  shutdown_ = true;
}

bool FileSystemDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  DCHECK(!shutdown_);
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileSystemDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_OpenFileSystemRequest, OnOpenFileSystem)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Move, OnMove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Copy, OnCopy)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Remove, OnRemove)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_ReadMetadata, OnReadMetadata)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Create, OnCreate)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Exists, OnExists)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_ReadDirectory, OnReadDirectory)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Write, OnWrite)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_Truncate, OnTruncate)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_TouchFile, OnTouchFile)
    IPC_MESSAGE_HANDLER(ViewHostMsg_FileSystem_CancelWrite, OnCancel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileSystemDispatcherHost::OnOpenFileSystem(
    int request_id, const GURL& origin_url, fileapi::FileSystemType type,
    int64 requested_size) {

  // TODO(kinuko): hook up ContentSettings cookies type checks.

  FilePath root_path;
  std::string name;

  if (!context_->GetFileSystemRootPath(origin_url,
                                       type,
                                       &root_path,
                                       &name)) {
    Send(new ViewMsg_OpenFileSystemRequest_Complete(
        request_id,
        false,
        std::string(),
        FilePath()));
    return;
  }

  // Run the completion task that creates the root directory and sends
  // back the status code to the dispatcher.
  OpenFileSystemCompletionTask::Run(request_id, name, root_path, this);
}

void FileSystemDispatcherHost::OnMove(
    int request_id, const FilePath& src_path, const FilePath& dest_path) {
  if (!CheckValidFileSystemPath(src_path, request_id) ||
      !CheckValidFileSystemPath(dest_path, request_id))
    return;

  GetNewOperation(request_id)->Move(src_path, dest_path);
}

void FileSystemDispatcherHost::OnCopy(
    int request_id, const FilePath& src_path, const FilePath& dest_path) {
  if (!CheckValidFileSystemPath(src_path, request_id) ||
      !CheckValidFileSystemPath(dest_path, request_id))
    return;

  GetNewOperation(request_id)->Copy(src_path, dest_path);
}

void FileSystemDispatcherHost::OnRemove(int request_id, const FilePath& path) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  GetNewOperation(request_id)->Remove(path);
}

void FileSystemDispatcherHost::OnReadMetadata(
    int request_id, const FilePath& path) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  GetNewOperation(request_id)->GetMetadata(path);
}

void FileSystemDispatcherHost::OnCreate(
    int request_id, const FilePath& path, bool exclusive,
    bool is_directory, bool recursive) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  if (is_directory)
    GetNewOperation(request_id)->CreateDirectory(path, exclusive, recursive);
  else
    GetNewOperation(request_id)->CreateFile(path, exclusive);
}

void FileSystemDispatcherHost::OnExists(
    int request_id, const FilePath& path, bool is_directory) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  if (is_directory)
    GetNewOperation(request_id)->DirectoryExists(path);
  else
    GetNewOperation(request_id)->FileExists(path);
}

void FileSystemDispatcherHost::OnReadDirectory(
    int request_id, const FilePath& path) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  GetNewOperation(request_id)->ReadDirectory(path);
}

void FileSystemDispatcherHost::OnWrite(
    int request_id,
    const FilePath& path,
    const GURL& blob_url,
    int64 offset) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  GetNewOperation(request_id)->Write(path, blob_url, offset);
}

void FileSystemDispatcherHost::OnTruncate(
    int request_id,
    const FilePath& path,
    int64 length) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  GetNewOperation(request_id)->Truncate(path, length);
}

void FileSystemDispatcherHost::OnTouchFile(
    int request_id,
    const FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!CheckValidFileSystemPath(path, request_id))
    return;
  GetNewOperation(request_id)->TouchFile(
      path, last_access_time, last_modified_time);
}

void FileSystemDispatcherHost::OnCancel(
    int request_id,
    int request_id_to_cancel) {
  fileapi::FileSystemOperation* write =
      operations_.Lookup(request_id_to_cancel);
  if (write) {
    write->Cancel();
    Send(new ViewMsg_FileSystem_DidSucceed(request_id));
  } else {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
  }
}

void FileSystemDispatcherHost::Send(IPC::Message* message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!shutdown_ && message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}

bool FileSystemDispatcherHost::CheckValidFileSystemPath(
    const FilePath& path, int request_id) {
  // We may want do more checks, but for now it just checks if the given
  // |path| is under the valid FileSystem root path for this host context.
  if (!context_->CheckValidFileSystemPath(path)) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_SECURITY));
    return false;
  }
  return true;
}

fileapi::FileSystemOperation* FileSystemDispatcherHost::GetNewOperation(
    int request_id) {
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(this, request_id);
  fileapi::FileSystemOperation* operation = new fileapi::FileSystemOperation(
      dispatcher,
      ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE));
  operations_.AddWithID(operation, request_id);
  return operation;
}

void FileSystemDispatcherHost::RemoveCompletedOperation(int request_id) {
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}
