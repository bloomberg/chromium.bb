// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/file_system_dispatcher_host.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/common/file_system_messages.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"

using fileapi::FileSystemCallbackDispatcher;
using fileapi::FileSystemOperation;

class BrowserFileSystemCallbackDispatcher
    : public FileSystemCallbackDispatcher {
 public:
  BrowserFileSystemCallbackDispatcher(
      FileSystemDispatcherHost* dispatcher_host, int request_id)
      : dispatcher_host_(dispatcher_host),
        request_id_(request_id) {
    DCHECK(dispatcher_host_);
  }

  virtual ~BrowserFileSystemCallbackDispatcher() {
    dispatcher_host_->UnregisterOperation(request_id_);
  }

  virtual void DidSucceed() {
    dispatcher_host_->Send(new FileSystemMsg_DidSucceed(request_id_));
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info) {
    dispatcher_host_->Send(new FileSystemMsg_DidReadMetadata(
        request_id_, info));
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
    dispatcher_host_->Send(new FileSystemMsg_DidReadDirectory(
        request_id_, entries, has_more));
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const FilePath& path) {
    dispatcher_host_->Send(
        new FileSystemMsg_OpenComplete(request_id_, !path.empty(), name, path));
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    dispatcher_host_->Send(new FileSystemMsg_DidFail(request_id_, error_code));
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    dispatcher_host_->Send(new FileSystemMsg_DidWrite(
        request_id_, bytes, complete));
  }

 private:
  scoped_refptr<FileSystemDispatcherHost> dispatcher_host_;
  int request_id_;
};

FileSystemDispatcherHost::FileSystemDispatcherHost(Profile* profile)
    : context_(profile->GetFileSystemContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()),
      request_context_getter_(profile->GetRequestContext()) {
}

FileSystemDispatcherHost::FileSystemDispatcherHost(
    ChromeURLRequestContext* context)
    : context_(context->file_system_context()),
      host_content_settings_map_(context->host_content_settings_map()),
      request_context_(context) {
}

FileSystemDispatcherHost::~FileSystemDispatcherHost() {
}

void FileSystemDispatcherHost::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  if (request_context_getter_.get()) {
    DCHECK(!request_context_.get());
    request_context_ = request_context_getter_->GetURLRequestContext();
  }
  DCHECK(request_context_.get());
}

bool FileSystemDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileSystemDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Open, OnOpen)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Move, OnMove)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Copy, OnCopy)
    IPC_MESSAGE_HANDLER(FileSystemMsg_Remove, OnRemove)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_ReadMetadata, OnReadMetadata)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Create, OnCreate)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Exists, OnExists)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_ReadDirectory, OnReadDirectory)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Write, OnWrite)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Truncate, OnTruncate)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_TouchFile, OnTouchFile)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_CancelWrite, OnCancel)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileSystemDispatcherHost::OnOpen(
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
    Send(new FileSystemMsg_OpenComplete(
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
  FileSystemOperation* write = operations_.Lookup(
      request_id_to_cancel);
  if (write) {
    // The cancel will eventually send both the write failure and the cancel
    // success.
    write->Cancel(GetNewOperation(request_id));
  } else {
    // The write already finished; report that we failed to stop it.
    Send(new FileSystemMsg_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
  }
}

FileSystemOperation* FileSystemDispatcherHost::GetNewOperation(
    int request_id) {
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(this, request_id);
  FileSystemOperation* operation = new FileSystemOperation(
      dispatcher,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      context_);
  operations_.AddWithID(operation, request_id);
  return operation;
}

void FileSystemDispatcherHost::UnregisterOperation(int request_id) {
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}
