// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_dispatcher_host.h"

#include "base/file_path.h"
#include "base/thread.h"
#include "base/time.h"
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

// A class to hold an ongoing openFileSystem completion task.
struct OpenFileSystemCompletionTask {
 public:
  static void Run(
      int request_id,
      int routing_id,
      const std::string& name,
      const FilePath& root_path,
      FileSystemDispatcherHost* dispatcher_host) {
    // The task is self-destructed.
    new OpenFileSystemCompletionTask(request_id, routing_id, name, root_path,
        dispatcher_host);
  }

  void DidFinish(base::PlatformFileError error) {
    if (error == base::PLATFORM_FILE_OK)
      dispatcher_host_->Send(
          new ViewMsg_OpenFileSystemRequest_Complete(
              routing_id_, request_id_, true, UTF8ToUTF16(name_),
              webkit_glue::FilePathToWebString(root_path_)));
    else
      dispatcher_host_->Send(
          new ViewMsg_OpenFileSystemRequest_Complete(
              routing_id_, request_id_, false, string16(), string16()));
    delete this;
  }

 private:
  OpenFileSystemCompletionTask(
      int request_id,
      int routing_id,
      const std::string& name,
      const FilePath& root_path,
      FileSystemDispatcherHost* dispatcher_host)
    : request_id_(request_id),
      routing_id_(routing_id),
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
  int routing_id_;
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

  // Drop all the operations.
  for (OperationsMap::const_iterator iter(&operations_);
       !iter.IsAtEnd(); iter.Advance()) {
    operations_.Remove(iter.GetCurrentKey());
  }
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

  // Run the completion task that creates the root directory and sends
  // back the status code to the dispatcher.
  OpenFileSystemCompletionTask::Run(
      params.request_id, params.routing_id, name, root_path, this);
}

void FileSystemDispatcherHost::OnMove(
    int request_id, const string16& src_path, const string16& dest_path) {
  FilePath src_file_path = webkit_glue::WebStringToFilePath(src_path);
  FilePath dest_file_path = webkit_glue::WebStringToFilePath(dest_path);

  if (!CheckValidFileSystemPath(src_file_path, request_id) ||
      !CheckValidFileSystemPath(dest_file_path, request_id))
    return;

  GetNewOperation(request_id)->Move(src_file_path, dest_file_path);
}

void FileSystemDispatcherHost::OnCopy(
    int request_id, const string16& src_path, const string16& dest_path) {
  FilePath src_file_path = webkit_glue::WebStringToFilePath(src_path);
  FilePath dest_file_path = webkit_glue::WebStringToFilePath(dest_path);

  if (!CheckValidFileSystemPath(src_file_path, request_id) ||
      !CheckValidFileSystemPath(dest_file_path, request_id))
    return;

  GetNewOperation(request_id)->Copy(src_file_path, dest_file_path);
}

void FileSystemDispatcherHost::OnRemove(
    int request_id, const string16& path) {
  FilePath file_path = webkit_glue::WebStringToFilePath(path);
  if (!CheckValidFileSystemPath(file_path, request_id))
    return;
  GetNewOperation(request_id)->Remove(file_path);
}

void FileSystemDispatcherHost::OnReadMetadata(
    int request_id, const string16& path) {
  FilePath file_path = webkit_glue::WebStringToFilePath(path);
  if (!CheckValidFileSystemPath(file_path, request_id))
    return;
  GetNewOperation(request_id)->GetMetadata(file_path);
}

void FileSystemDispatcherHost::OnCreate(
    int request_id, const string16& path, bool exclusive, bool is_directory) {
  FilePath file_path = webkit_glue::WebStringToFilePath(path);
  if (!CheckValidFileSystemPath(file_path, request_id))
    return;
  if (is_directory)
    GetNewOperation(request_id)->CreateDirectory(file_path, exclusive);
  else
    GetNewOperation(request_id)->CreateFile(file_path, exclusive);
}

void FileSystemDispatcherHost::OnExists(
    int request_id, const string16& path, bool is_directory) {
  FilePath file_path = webkit_glue::WebStringToFilePath(path);
  if (!CheckValidFileSystemPath(file_path, request_id))
    return;
  if (is_directory)
    GetNewOperation(request_id)->DirectoryExists(file_path);
  else
    GetNewOperation(request_id)->FileExists(file_path);
}

void FileSystemDispatcherHost::OnReadDirectory(
    int request_id, const string16& path) {
  FilePath file_path = webkit_glue::WebStringToFilePath(path);
  if (!CheckValidFileSystemPath(file_path, request_id))
    return;
  GetNewOperation(request_id)->ReadDirectory(file_path);
}

void FileSystemDispatcherHost::DidFail(
    WebKit::WebFileError status,
    fileapi::FileSystemOperation* operation) {
  int request_id =
      static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  Send(new ViewMsg_FileSystem_DidFail(request_id, status));
  operations_.Remove(request_id);
}

void FileSystemDispatcherHost::DidSucceed(
    fileapi::FileSystemOperation* operation) {
  int request_id =
      static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  Send(new ViewMsg_FileSystem_DidSucceed(request_id));
  operations_.Remove(request_id);
}

void FileSystemDispatcherHost::DidReadMetadata(
    const base::PlatformFileInfo& info,
    fileapi::FileSystemOperation* operation) {
  int request_id =
      static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  Send(new ViewMsg_FileSystem_DidReadMetadata(request_id, info));
  operations_.Remove(request_id);
}

void FileSystemDispatcherHost::DidReadDirectory(
    const std::vector<base::file_util_proxy::Entry>& entries,
    bool has_more,
    fileapi::FileSystemOperation* operation) {
  int request_id =
      static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  ViewMsg_FileSystem_DidReadDirectory_Params params;
  params.request_id = request_id;
  params.entries = entries;
  params.has_more = has_more;
  Send(new ViewMsg_FileSystem_DidReadDirectory(params));
  operations_.Remove(request_id);
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
        request_id, WebKit::WebFileErrorSecurity));
    return false;
  }
  return true;
}

ChromeFileSystemOperation* FileSystemDispatcherHost::GetNewOperation(
    int request_id) {
  scoped_ptr<ChromeFileSystemOperation> operation(
      new ChromeFileSystemOperation(request_id, this));
  operations_.AddWithID(operation.get(), request_id);
  return operation.release();
}
