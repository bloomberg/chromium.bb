// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/file_system_dispatcher_host.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/platform_file.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/resource_context.h"
#include "content/common/file_system_messages.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_platform_file.h"
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

  virtual void DidReadMetadata(
      const base::PlatformFileInfo& info,
      const FilePath& platform_path) {
    dispatcher_host_->Send(new FileSystemMsg_DidReadMetadata(
        request_id_, info, platform_path));
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries, bool has_more) {
    dispatcher_host_->Send(new FileSystemMsg_DidReadDirectory(
        request_id_, entries, has_more));
  }

  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) {
    dispatcher_host_->Send(
        new FileSystemMsg_OpenComplete(
            request_id_, root.is_valid(), name, root));
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    dispatcher_host_->Send(new FileSystemMsg_DidFail(request_id_, error_code));
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    dispatcher_host_->Send(new FileSystemMsg_DidWrite(
        request_id_, bytes, complete));
  }

  virtual void DidOpenFile(
      base::PlatformFile file,
      base::ProcessHandle peer_handle) {
    IPC::PlatformFileForTransit file_for_transit =
        IPC::InvalidPlatformFileForTransit();
    if (file != base::kInvalidPlatformFileValue) {
#if defined(OS_WIN)
      if (!::DuplicateHandle(::GetCurrentProcess(), file, peer_handle,
                             &file_for_transit, 0, false,
                             DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE))
        file_for_transit = IPC::InvalidPlatformFileForTransit();
#elif defined(OS_POSIX)
      file_for_transit = base::FileDescriptor(file, true);
#else
  #error Not implemented.
#endif
    }
    dispatcher_host_->Send(new FileSystemMsg_DidOpenFile(
        request_id_, file_for_transit));
  }

 private:
  scoped_refptr<FileSystemDispatcherHost> dispatcher_host_;
  int request_id_;
};

FileSystemDispatcherHost::FileSystemDispatcherHost(
    const content::ResourceContext* resource_context)
    : context_(NULL),
      resource_context_(resource_context),
      request_context_(NULL) {
  DCHECK(resource_context_);
}

FileSystemDispatcherHost::FileSystemDispatcherHost(
    net::URLRequestContext* request_context,
    fileapi::FileSystemContext* file_system_context)
    : context_(file_system_context),
      resource_context_(NULL),
      request_context_(request_context) {
  DCHECK(request_context_);
  DCHECK(context_);
}

FileSystemDispatcherHost::~FileSystemDispatcherHost() {
}

void FileSystemDispatcherHost::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  if (resource_context_) {
    DCHECK(!request_context_);
    request_context_ = resource_context_->request_context();
    DCHECK(!context_);
    context_ = resource_context_->file_system_context();
    resource_context_ = NULL;
  }
  DCHECK(request_context_);
  DCHECK(context_);
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
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_OpenFile, OnOpenFile)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileSystemDispatcherHost::OnOpen(
    int request_id, const GURL& origin_url, fileapi::FileSystemType type,
    int64 requested_size, bool create) {
  GetNewOperation(request_id)->OpenFileSystem(origin_url, type, create);
}

void FileSystemDispatcherHost::OnMove(
    int request_id, const GURL& src_path, const GURL& dest_path) {
  GetNewOperation(request_id)->Move(src_path, dest_path);
}

void FileSystemDispatcherHost::OnCopy(
    int request_id, const GURL& src_path, const GURL& dest_path) {
  GetNewOperation(request_id)->Copy(src_path, dest_path);
}

void FileSystemDispatcherHost::OnRemove(
    int request_id, const GURL& path, bool recursive) {
  GetNewOperation(request_id)->Remove(path, recursive);
}

void FileSystemDispatcherHost::OnReadMetadata(
    int request_id, const GURL& path) {
  GetNewOperation(request_id)->GetMetadata(path);
}

void FileSystemDispatcherHost::OnCreate(
    int request_id, const GURL& path, bool exclusive,
    bool is_directory, bool recursive) {
  if (is_directory)
    GetNewOperation(request_id)->CreateDirectory(path, exclusive, recursive);
  else
    GetNewOperation(request_id)->CreateFile(path, exclusive);
}

void FileSystemDispatcherHost::OnExists(
    int request_id, const GURL& path, bool is_directory) {
  if (is_directory)
    GetNewOperation(request_id)->DirectoryExists(path);
  else
    GetNewOperation(request_id)->FileExists(path);
}

void FileSystemDispatcherHost::OnReadDirectory(
    int request_id, const GURL& path) {
  GetNewOperation(request_id)->ReadDirectory(path);
}

void FileSystemDispatcherHost::OnWrite(
    int request_id,
    const GURL& path,
    const GURL& blob_url,
    int64 offset) {
  GetNewOperation(request_id)->Write(
      request_context_, path, blob_url, offset);
}

void FileSystemDispatcherHost::OnTruncate(
    int request_id,
    const GURL& path,
    int64 length) {
  GetNewOperation(request_id)->Truncate(path, length);
}

void FileSystemDispatcherHost::OnTouchFile(
    int request_id,
    const GURL& path,
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

void FileSystemDispatcherHost::OnOpenFile(
    int request_id, const GURL& path, int file_flags) {
  GetNewOperation(request_id)->OpenFile(path, file_flags, peer_handle());
}

FileSystemOperation* FileSystemDispatcherHost::GetNewOperation(
    int request_id) {
  BrowserFileSystemCallbackDispatcher* dispatcher =
      new BrowserFileSystemCallbackDispatcher(this, request_id);
  FileSystemOperation* operation = new FileSystemOperation(
      dispatcher,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      context_,
      NULL);
  operations_.AddWithID(operation, request_id);
  return operation;
}

void FileSystemDispatcherHost::UnregisterOperation(int request_id) {
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}
