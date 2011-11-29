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
#include "content/browser/user_metrics.h"
#include "content/common/file_system_messages.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_platform_file.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"

using content::BrowserThread;
using fileapi::FileSystemCallbackDispatcher;
using fileapi::FileSystemFileUtil;
using fileapi::FileSystemOperation;
using fileapi::FileSystemOperationContext;

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
        file != base::kInvalidPlatformFileValue ?
            IPC::GetFileHandleForProcess(file, peer_handle, true) :
            IPC::InvalidPlatformFileForTransit();

    dispatcher_host_->Send(new FileSystemMsg_DidOpenFile(
        request_id_, file_for_transit));
  }

 private:
  scoped_refptr<FileSystemDispatcherHost> dispatcher_host_;
  int request_id_;
};

FileSystemDispatcherHost::FileSystemDispatcherHost(
    net::URLRequestContextGetter* request_context_getter,
    fileapi::FileSystemContext* file_system_context)
    : context_(file_system_context),
      request_context_getter_(request_context_getter),
      request_context_(NULL) {
  DCHECK(context_);
  DCHECK(request_context_getter_);
}

FileSystemDispatcherHost::FileSystemDispatcherHost(
    net::URLRequestContext* request_context,
    fileapi::FileSystemContext* file_system_context)
    : context_(file_system_context),
      request_context_(request_context) {
  DCHECK(context_);
  DCHECK(request_context_);
}

FileSystemDispatcherHost::~FileSystemDispatcherHost() {
}

void FileSystemDispatcherHost::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  if (request_context_getter_.get()) {
    DCHECK(!request_context_);
    request_context_ = request_context_getter_->GetURLRequestContext();
    request_context_getter_ = NULL;
    DCHECK(request_context_);
  }
}

void FileSystemDispatcherHost::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == FileSystemHostMsg_SyncGetPlatformPath::ID)
    *thread = BrowserThread::FILE;
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
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_WillUpdate, OnWillUpdate)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_DidUpdate, OnDidUpdate)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_SyncGetPlatformPath,
                        OnSyncGetPlatformPath)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileSystemDispatcherHost::OnOpen(
    int request_id, const GURL& origin_url, fileapi::FileSystemType type,
    int64 requested_size, bool create) {
  if (type == fileapi::kFileSystemTypeTemporary) {
    UserMetrics::RecordAction(UserMetricsAction("OpenFileSystemTemporary"));
  } else if (type == fileapi::kFileSystemTypePersistent) {
    UserMetrics::RecordAction(UserMetricsAction("OpenFileSystemPersistent"));
  }
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
  if (!request_context_) {
    // We can't write w/o a request context, trying to do so will crash.
    NOTREACHED();
    return;
  }
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

void FileSystemDispatcherHost::OnWillUpdate(const GURL& path) {
  GURL origin_url;
  fileapi::FileSystemType type;
  if (!CrackFileSystemURL(path, &origin_url, &type, NULL))
    return;
  fileapi::FileSystemQuotaUtil* quota_util = context_->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->proxy()->StartUpdateOrigin(origin_url, type);
}

void FileSystemDispatcherHost::OnDidUpdate(const GURL& path, int64 delta) {
  GURL origin_url;
  fileapi::FileSystemType type;
  if (!CrackFileSystemURL(path, &origin_url, &type, NULL))
    return;
  fileapi::FileSystemQuotaUtil* quota_util = context_->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->proxy()->UpdateOriginUsage(
      context_->quota_manager_proxy(), origin_url, type, delta);
  quota_util->proxy()->EndUpdateOrigin(origin_url, type);
}

void FileSystemDispatcherHost::OnSyncGetPlatformPath(
    const GURL& path, FilePath* platform_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(platform_path);
  *platform_path = FilePath();

  FileSystemOperation* operation = new FileSystemOperation(
      NULL,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      context_);

  operation->SyncGetPlatformPath(path, platform_path);
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
