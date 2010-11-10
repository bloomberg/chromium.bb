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
#include "chrome/browser/file_system/browser_file_system_context.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota_manager.h"
#include "webkit/fileapi/sandboxed_file_system_operation.h"

using fileapi::FileSystemQuotaManager;
using fileapi::SandboxedFileSystemOperation;

FileSystemDispatcherHost::FileSystemDispatcherHost(
    IPC::Message::Sender* sender, Profile* profile)
    : message_sender_(sender),
      process_handle_(0),
      shutdown_(false),
      context_(profile->GetFileSystemContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()),
      request_context_getter_(profile->GetRequestContext()) {
  DCHECK(message_sender_);
}

FileSystemDispatcherHost::FileSystemDispatcherHost(
    IPC::Message::Sender* sender, ChromeURLRequestContext* context)
    : message_sender_(sender),
      process_handle_(0),
      shutdown_(false),
      context_(context->browser_file_system_context()),
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

  GetNewOperation(request_id)->OpenFileSystem(origin_url, type, create);
}

void FileSystemDispatcherHost::OnMove(
    int request_id, const FilePath& src_path, const FilePath& dest_path) {
  GetNewOperation(request_id)->Move(src_path, dest_path);
}

void FileSystemDispatcherHost::OnCopy(
    int request_id, const FilePath& src_path, const FilePath& dest_path) {
  GetNewOperation(request_id)->Copy(src_path, dest_path);
}

void FileSystemDispatcherHost::OnRemove(
    int request_id, const FilePath& path, bool recursive) {
  GetNewOperation(request_id)->Remove(path, recursive);
}

void FileSystemDispatcherHost::OnReadMetadata(
    int request_id, const FilePath& path) {
  GetNewOperation(request_id)->GetMetadata(path);
}

void FileSystemDispatcherHost::OnCreate(
    int request_id, const FilePath& path, bool exclusive,
    bool is_directory, bool recursive) {
  if (is_directory)
    GetNewOperation(request_id)->CreateDirectory(path, exclusive, recursive);
  else
    GetNewOperation(request_id)->CreateFile(path, exclusive);
}

void FileSystemDispatcherHost::OnExists(
    int request_id, const FilePath& path, bool is_directory) {
  if (is_directory)
    GetNewOperation(request_id)->DirectoryExists(path);
  else
    GetNewOperation(request_id)->FileExists(path);
}

void FileSystemDispatcherHost::OnReadDirectory(
    int request_id, const FilePath& path) {
  GetNewOperation(request_id)->ReadDirectory(path);
}

void FileSystemDispatcherHost::OnWrite(
    int request_id,
    const FilePath& path,
    const GURL& blob_url,
    int64 offset) {
  GetNewOperation(request_id)->Write(
      request_context_, path, blob_url, offset);
}

void FileSystemDispatcherHost::OnTruncate(
    int request_id,
    const FilePath& path,
    int64 length) {
  GetNewOperation(request_id)->Truncate(path, length);
}

void FileSystemDispatcherHost::OnTouchFile(
    int request_id,
    const FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  GetNewOperation(request_id)->TouchFile(
      path, last_access_time, last_modified_time);
}

void FileSystemDispatcherHost::OnCancel(
    int request_id,
    int request_id_to_cancel) {
  SandboxedFileSystemOperation* write = operations_.Lookup(
      request_id_to_cancel);
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

SandboxedFileSystemOperation* FileSystemDispatcherHost::GetNewOperation(
    int request_id) {
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(this, request_id);
  SandboxedFileSystemOperation* operation = new SandboxedFileSystemOperation(
      dispatcher,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      context_.get());
  operations_.AddWithID(operation, request_id);
  return operation;
}

void FileSystemDispatcherHost::RemoveCompletedOperation(int request_id) {
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}
