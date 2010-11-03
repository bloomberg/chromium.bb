// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_dispatcher_host.h"

#include "base/file_path.h"
#include "base/thread.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/file_system/browser_file_system_callback_dispatcher.h"
#include "chrome/browser/file_system/file_system_host_context.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota.h"

using fileapi::FileSystemQuota;

class FileSystemDispatcherHost::OpenFileSystemTask {
 public:
  static void Start(
      int request_id,
      const GURL& origin_url,
      fileapi::FileSystemType type,
      bool create,
      FileSystemDispatcherHost* dispatcher_host) {
    // The task is self-destructed.
    new OpenFileSystemTask(
        request_id, origin_url, type, create, dispatcher_host);
  }

 private:
  void DidGetRootPath(bool success, const FilePath& root_path,
                      const std::string& name) {
    if (success)
      dispatcher_host_->Send(
          new ViewMsg_OpenFileSystemRequest_Complete(
              request_id_, true, name, root_path));
    else
      dispatcher_host_->Send(
          new ViewMsg_OpenFileSystemRequest_Complete(
              request_id_, false, std::string(), FilePath()));
    delete this;
  }

  OpenFileSystemTask(
      int request_id,
      const GURL& origin_url,
      fileapi::FileSystemType type,
      bool create,
      FileSystemDispatcherHost* dispatcher_host)
    : request_id_(request_id),
      dispatcher_host_(dispatcher_host),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    dispatcher_host->context_->path_manager()->GetFileSystemRootPath(
        origin_url, type, create,
        callback_factory_.NewCallback(&OpenFileSystemTask::DidGetRootPath));
  }

  int request_id_;
  std::string name_;
  FilePath root_path_;
  scoped_refptr<FileSystemDispatcherHost> dispatcher_host_;
  base::ScopedCallbackFactory<OpenFileSystemTask> callback_factory_;
};

FileSystemDispatcherHost::FileSystemDispatcherHost(
    IPC::Message::Sender* sender, Profile* profile)
    : message_sender_(sender),
      process_handle_(0),
      shutdown_(false),
      context_(profile->GetFileSystemHostContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()),
      request_context_getter_(profile->GetRequestContext()) {
  DCHECK(message_sender_);
}

FileSystemDispatcherHost::FileSystemDispatcherHost(
    IPC::Message::Sender* sender, ChromeURLRequestContext* context)
    : message_sender_(sender),
      process_handle_(0),
      shutdown_(false),
      context_(context->file_system_host_context()),
      host_content_settings_map_(context->host_content_settings_map()),
      request_context_(context) {
  DCHECK(message_sender_);
}

FileSystemDispatcherHost::~FileSystemDispatcherHost() {
}

void FileSystemDispatcherHost::Init(base::ProcessHandle process_handle) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!shutdown_);
  DCHECK(!process_handle_);
  DCHECK(process_handle);
  process_handle_ = process_handle;
  if (request_context_getter_.get()) {
    DCHECK(!request_context_.get());
    request_context_ = request_context_getter_->GetURLRequestContext();
  }
  DCHECK(request_context_.get());
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
    int64 requested_size, bool create) {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          origin_url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  DCHECK((content_setting == CONTENT_SETTING_ALLOW) ||
         (content_setting == CONTENT_SETTING_BLOCK) ||
         (content_setting == CONTENT_SETTING_SESSION_ONLY));
  if (content_setting == CONTENT_SETTING_BLOCK) {
    // TODO(kinuko): Need to notify the UI thread to indicate that
    // there's a blocked content.
    Send(new ViewMsg_OpenFileSystemRequest_Complete(
        request_id, false, std::string(), FilePath()));
    return;
  }

  OpenFileSystemTask::Start(request_id, origin_url, type, create, this);
}

void FileSystemDispatcherHost::OnMove(
    int request_id, const FilePath& src_path, const FilePath& dest_path) {
  if (!VerifyFileSystemPathForRead(src_path, request_id) ||
      !VerifyFileSystemPathForWrite(dest_path, request_id, true /* create */,
                                    FileSystemQuota::kUnknownSize))
    return;

  GetNewOperation(request_id)->Move(src_path, dest_path);
}

void FileSystemDispatcherHost::OnCopy(
    int request_id, const FilePath& src_path, const FilePath& dest_path) {
  if (!VerifyFileSystemPathForRead(src_path, request_id) ||
      !VerifyFileSystemPathForWrite(dest_path, request_id, true /* create */,
                                    FileSystemQuota::kUnknownSize))
    return;

  GetNewOperation(request_id)->Copy(src_path, dest_path);
}

void FileSystemDispatcherHost::OnRemove(
    int request_id, const FilePath& path, bool recursive) {
  if (!VerifyFileSystemPathForWrite(path, request_id, false /* create */, 0))
    return;
  GetNewOperation(request_id)->Remove(path, recursive);
}

void FileSystemDispatcherHost::OnReadMetadata(
    int request_id, const FilePath& path) {
  if (!VerifyFileSystemPathForRead(path, request_id))
    return;
  GetNewOperation(request_id)->GetMetadata(path);
}

void FileSystemDispatcherHost::OnCreate(
    int request_id, const FilePath& path, bool exclusive,
    bool is_directory, bool recursive) {
  if (!VerifyFileSystemPathForWrite(path, request_id, true /* create */, 0))
    return;
  if (is_directory)
    GetNewOperation(request_id)->CreateDirectory(path, exclusive, recursive);
  else
    GetNewOperation(request_id)->CreateFile(path, exclusive);
}

void FileSystemDispatcherHost::OnExists(
    int request_id, const FilePath& path, bool is_directory) {
  if (!VerifyFileSystemPathForRead(path, request_id))
    return;
  if (is_directory)
    GetNewOperation(request_id)->DirectoryExists(path);
  else
    GetNewOperation(request_id)->FileExists(path);
}

void FileSystemDispatcherHost::OnReadDirectory(
    int request_id, const FilePath& path) {
  if (!VerifyFileSystemPathForRead(path, request_id))
    return;
  GetNewOperation(request_id)->ReadDirectory(path);
}

void FileSystemDispatcherHost::OnWrite(
    int request_id,
    const FilePath& path,
    const GURL& blob_url,
    int64 offset) {
  if (!VerifyFileSystemPathForWrite(path, request_id, true /* create */,
                                    FileSystemQuota::kUnknownSize))
    return;
  GetNewOperation(request_id)->Write(
      request_context_, path, blob_url, offset);
}

void FileSystemDispatcherHost::OnTruncate(
    int request_id,
    const FilePath& path,
    int64 length) {
  if (!VerifyFileSystemPathForWrite(path, request_id, false /* create */, 0))
    return;
  GetNewOperation(request_id)->Truncate(path, length);
}

void FileSystemDispatcherHost::OnTouchFile(
    int request_id,
    const FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!VerifyFileSystemPathForWrite(path, request_id, true /* create */, 0))
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
    // The cancel will eventually send both the write failure and the cancel
    // success.
    write->Cancel(GetNewOperation(request_id));
  } else {
    // The write already finished; report that we failed to stop it.
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
  }
}

void FileSystemDispatcherHost::Send(IPC::Message* message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!shutdown_ && message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}

bool FileSystemDispatcherHost::VerifyFileSystemPathForRead(
    const FilePath& path, int request_id) {
  // We may want do more checks, but for now it just checks if the given
  // |path| is under the valid FileSystem root path for this host context.
  if (!context_->path_manager()->CrackFileSystemPath(
          path, NULL, NULL, NULL)) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_SECURITY));
    return false;
  }
  return true;
}

bool FileSystemDispatcherHost::VerifyFileSystemPathForWrite(
    const FilePath& path, int request_id, bool create, int64 growth) {
  GURL origin_url;
  FilePath virtual_path;
  if (!context_->path_manager()->CrackFileSystemPath(
          path, &origin_url, NULL, &virtual_path)) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_SECURITY));
    return false;
  }
  // Any write access is disallowed on the root path.
  if (virtual_path.value().length() == 0 ||
      virtual_path.DirName().value() == virtual_path.value()) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_SECURITY));
    return false;
  }
  if (create && context_->path_manager()->IsRestrictedFileName(
          path.BaseName())) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_SECURITY));
    return false;
  }
  // TODO(kinuko): For operations with kUnknownSize we'll eventually
  // need to resolve what amount of size it's going to write.
  if (!context_->CheckOriginQuota(origin_url, growth)) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_NO_SPACE));
    return false;
  }
  return true;
}

bool FileSystemDispatcherHost::CheckIfFilePathIsSafe(
    const FilePath& path, int request_id) {
  if (context_->path_manager()->IsRestrictedFileName(path.BaseName())) {
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
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  operations_.AddWithID(operation, request_id);
  return operation;
}

void FileSystemDispatcherHost::RemoveCompletedOperation(int request_id) {
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}
