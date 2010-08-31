// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_dispatcher_host.h"

#include "base/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/file_system/file_system_host_context.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"
#include "webkit/glue/webkit_glue.h"

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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileSystemDispatcherHost::OnOpenFileSystem(
    const ViewHostMsg_OpenFileSystemRequest_Params& params) {

  // TODO(kinuko): hook up ContentSettings cookies type checks.

  FilePath root_path;
  std::string name;

  if (!context_->GetFileSystemRootPath(params.origin_url,
                                       params.type,
                                       &root_path,
                                       &name)) {
    Send(new ViewMsg_OpenFileSystemRequest_Complete(
        params.routing_id,
        params.request_id,
        false,
        string16(),
        string16()));
    return;
  }

  // TODO(kinuko): creates the root directory and if it succeeds.

  Send(new ViewMsg_OpenFileSystemRequest_Complete(
      params.routing_id,
      params.request_id,
      true,
      UTF8ToUTF16(name),
      webkit_glue::FilePathToWebString(root_path)));
}

void FileSystemDispatcherHost::OnMove(
    int request_id, const string16& src_path, const string16& dest_path) {
  if (!context_->CheckValidFileSystemPath(
          webkit_glue::WebStringToFilePath(src_path)) ||
      !context_->CheckValidFileSystemPath(
          webkit_glue::WebStringToFilePath(dest_path))) {
    Send(new ViewMsg_FileSystem_DidFail(
        request_id, WebKit::WebFileErrorSecurity));
    return;
  }

  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::OnCopy(
    int request_id,
    const string16& src_path,
    const string16& dest_path) {
  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::OnRemove(
    int request_id,
    const string16& path) {
  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::OnReadMetadata(
    int request_id,
    const string16& path) {
  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::OnCreate(
    int request_id,
    const string16& path,
    bool exclusive,
    bool is_directory) {
  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::OnExists(
    int request_id,
    const string16& path,
    bool is_directory) {
  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::OnReadDirectory(
    int request_id,
    const string16& path) {
  // TODO(kinuko): not implemented yet.
  Send(new ViewMsg_FileSystem_DidFail(
      request_id, WebKit::WebFileErrorAbort));
}

void FileSystemDispatcherHost::Send(IPC::Message* message) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    if (!ChromeThread::PostTask(
            ChromeThread::IO, FROM_HERE,
            NewRunnableMethod(this,
                              &FileSystemDispatcherHost::Send,
                              message)))
      delete message;
    return;
  }

  if (!shutdown_ && message_sender_)
    message_sender_->Send(message);
  else
    delete message;
}
