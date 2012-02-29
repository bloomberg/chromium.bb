// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/file_and_blob_message_filter.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/chrome_blob_storage_context.h"
#include "content/common/file_system_messages.h"
#include "content/common/webblob_messages.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/deletable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

using content::BrowserMessageFilter;
using content::BrowserThread;
using content::UserMetricsAction;
using fileapi::FileSystemFileUtil;
using fileapi::FileSystemOperation;
using fileapi::FileSystemOperationInterface;
using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;

FileAndBlobMessageFilter::FileAndBlobMessageFilter(
    int process_id,
    net::URLRequestContextGetter* request_context_getter,
    fileapi::FileSystemContext* file_system_context,
    ChromeBlobStorageContext* blob_storage_context)
    : process_id_(process_id),
      context_(file_system_context),
      request_context_getter_(request_context_getter),
      request_context_(NULL),
      blob_storage_context_(blob_storage_context) {
  DCHECK(context_);
  DCHECK(request_context_getter_);
  DCHECK(blob_storage_context);
}

FileAndBlobMessageFilter::FileAndBlobMessageFilter(
    int process_id,
    net::URLRequestContext* request_context,
    fileapi::FileSystemContext* file_system_context,
    ChromeBlobStorageContext* blob_storage_context)
    : process_id_(process_id),
      context_(file_system_context),
      request_context_(request_context),
      blob_storage_context_(blob_storage_context) {
  DCHECK(context_);
  DCHECK(request_context_);
  DCHECK(blob_storage_context);
}

FileAndBlobMessageFilter::~FileAndBlobMessageFilter() {
}

void FileAndBlobMessageFilter::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  if (request_context_getter_.get()) {
    DCHECK(!request_context_);
    request_context_ = request_context_getter_->GetURLRequestContext();
    request_context_getter_ = NULL;
    DCHECK(request_context_);
  }
}

void FileAndBlobMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Unregister all the blob URLs that are previously registered in this
  // process.
  for (base::hash_set<std::string>::const_iterator iter = blob_urls_.begin();
       iter != blob_urls_.end(); ++iter) {
    blob_storage_context_->controller()->RemoveBlob(GURL(*iter));
  }
}

void FileAndBlobMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == FileSystemHostMsg_SyncGetPlatformPath::ID)
    *thread = BrowserThread::FILE;
}

bool FileAndBlobMessageFilter::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileAndBlobMessageFilter, message, *message_was_ok)
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
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_CreateSnapshotFile,
                        OnCreateSnapshotFile)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_WillUpdate, OnWillUpdate)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_DidUpdate, OnDidUpdate)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_SyncGetPlatformPath,
                        OnSyncGetPlatformPath)
    IPC_MESSAGE_HANDLER(BlobHostMsg_StartBuildingBlob, OnStartBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_AppendBlobDataItem, OnAppendBlobDataItem)
    IPC_MESSAGE_HANDLER(BlobHostMsg_SyncAppendSharedMemory,
                        OnAppendSharedMemory)
    IPC_MESSAGE_HANDLER(BlobHostMsg_FinishBuildingBlob, OnFinishBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_CloneBlob, OnCloneBlob)
    IPC_MESSAGE_HANDLER(BlobHostMsg_RemoveBlob, OnRemoveBlob)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void FileAndBlobMessageFilter::OnOpen(
    int request_id, const GURL& origin_url, fileapi::FileSystemType type,
    int64 requested_size, bool create) {
  if (type == fileapi::kFileSystemTypeTemporary) {
    content::RecordAction(UserMetricsAction("OpenFileSystemTemporary"));
  } else if (type == fileapi::kFileSystemTypePersistent) {
    content::RecordAction(UserMetricsAction("OpenFileSystemPersistent"));
  }
  context_->OpenFileSystem(origin_url, type, create, base::Bind(
      &FileAndBlobMessageFilter::DidOpenFileSystem, this, request_id));
}

void FileAndBlobMessageFilter::OnMove(
    int request_id, const GURL& src_path, const GURL& dest_path) {
  GetNewOperation(src_path, request_id)->Move(
      src_path, dest_path,
      base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
}

void FileAndBlobMessageFilter::OnCopy(
    int request_id, const GURL& src_path, const GURL& dest_path) {
  GetNewOperation(src_path, request_id)->Copy(
      src_path, dest_path,
      base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
}

void FileAndBlobMessageFilter::OnRemove(
    int request_id, const GURL& path, bool recursive) {
  GetNewOperation(path, request_id)->Remove(
      path, recursive,
      base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
}

void FileAndBlobMessageFilter::OnReadMetadata(
    int request_id, const GURL& path) {
  GetNewOperation(path, request_id)->GetMetadata(
      path,
      base::Bind(&FileAndBlobMessageFilter::DidGetMetadata, this, request_id));
}

void FileAndBlobMessageFilter::OnCreate(
    int request_id, const GURL& path, bool exclusive,
    bool is_directory, bool recursive) {
  if (is_directory) {
    GetNewOperation(path, request_id)->CreateDirectory(
        path, exclusive, recursive,
        base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
  } else {
    GetNewOperation(path, request_id)->CreateFile(
        path, exclusive,
        base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
  }
}

void FileAndBlobMessageFilter::OnExists(
    int request_id, const GURL& path, bool is_directory) {
  if (is_directory) {
    GetNewOperation(path, request_id)->DirectoryExists(
        path,
        base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
  } else {
    GetNewOperation(path, request_id)->FileExists(
        path,
        base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
  }
}

void FileAndBlobMessageFilter::OnReadDirectory(
    int request_id, const GURL& path) {
  GetNewOperation(path, request_id)->ReadDirectory(
      path, base::Bind(&FileAndBlobMessageFilter::DidReadDirectory,
                       this, request_id));
}

void FileAndBlobMessageFilter::OnWrite(
    int request_id,
    const GURL& path,
    const GURL& blob_url,
    int64 offset) {
  if (!request_context_) {
    // We can't write w/o a request context, trying to do so will crash.
    NOTREACHED();
    return;
  }
  GetNewOperation(path, request_id)->Write(
      request_context_, path, blob_url, offset,
      base::Bind(&FileAndBlobMessageFilter::DidWrite, this, request_id));
}

void FileAndBlobMessageFilter::OnTruncate(
    int request_id,
    const GURL& path,
    int64 length) {
  GetNewOperation(path, request_id)->Truncate(
      path, length,
      base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
}

void FileAndBlobMessageFilter::OnTouchFile(
    int request_id,
    const GURL& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  GetNewOperation(path, request_id)->TouchFile(
      path, last_access_time, last_modified_time,
      base::Bind(&FileAndBlobMessageFilter::DidFinish, this, request_id));
}

void FileAndBlobMessageFilter::OnCancel(
    int request_id,
    int request_id_to_cancel) {
  FileSystemOperationInterface* write = operations_.Lookup(
      request_id_to_cancel);
  if (write) {
    // The cancel will eventually send both the write failure and the cancel
    // success.
    write->Cancel(
        base::Bind(&FileAndBlobMessageFilter::DidCancel, this, request_id));
  } else {
    // The write already finished; report that we failed to stop it.
    Send(new FileSystemMsg_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
  }
}

void FileAndBlobMessageFilter::OnOpenFile(
    int request_id, const GURL& path, int file_flags) {
  GetNewOperation(path, request_id)->OpenFile(
      path, file_flags, peer_handle(),
      base::Bind(&FileAndBlobMessageFilter::DidOpenFile, this, request_id));
}

void FileAndBlobMessageFilter::OnWillUpdate(const GURL& path) {
  GURL origin_url;
  fileapi::FileSystemType type;
  if (!CrackFileSystemURL(path, &origin_url, &type, NULL))
    return;
  fileapi::FileSystemQuotaUtil* quota_util = context_->GetQuotaUtil(type);
  if (!quota_util)
    return;
  quota_util->proxy()->StartUpdateOrigin(origin_url, type);
}

void FileAndBlobMessageFilter::OnDidUpdate(const GURL& path, int64 delta) {
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

void FileAndBlobMessageFilter::OnSyncGetPlatformPath(
    const GURL& path, FilePath* platform_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(platform_path);
  *platform_path = FilePath();

  GURL origin_url;
  fileapi::FileSystemType file_system_type = fileapi::kFileSystemTypeUnknown;
  FilePath file_path;
  if (!fileapi::CrackFileSystemURL(
          path, &origin_url, &file_system_type, &file_path)) {
    return;
  }

  // This is called only by pepper plugin as of writing to get the
  // underlying platform path to upload a file in the sandboxed filesystem
  // (e.g. TEMPORARY or PERSISTENT).
  // TODO(kinuko): this hack should go away once appropriate upload-stream
  // handling based on element types is supported.
  FileSystemOperation* operation =
      context_->CreateFileSystemOperation(
          path,
          BrowserThread::GetMessageLoopProxyForThread(
              BrowserThread::FILE))->AsFileSystemOperation();
  DCHECK(operation);
  operation->SyncGetPlatformPath(path, platform_path);
}

void FileAndBlobMessageFilter::OnCreateSnapshotFile(
    int request_id, const GURL& blob_url, const GURL& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GetNewOperation(path, request_id)->CreateSnapshotFile(
      path,
      base::Bind(&FileAndBlobMessageFilter::DidCreateSnapshot,
                 this, request_id, blob_url));
}

void FileAndBlobMessageFilter::OnStartBuildingBlob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->StartBuildingBlob(url);
  blob_urls_.insert(url.spec());
}

void FileAndBlobMessageFilter::OnAppendBlobDataItem(
    const GURL& url, const BlobData::Item& item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (item.type == BlobData::TYPE_FILE &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          process_id_, item.file_path)) {
    OnRemoveBlob(url);
    return;
  }
  blob_storage_context_->controller()->AppendBlobDataItem(url, item);
}

void FileAndBlobMessageFilter::OnAppendSharedMemory(
    const GURL& url, base::SharedMemoryHandle handle, size_t buffer_size) {
  DCHECK(base::SharedMemory::IsHandleValid(handle));
#if defined(OS_WIN)
  base::SharedMemory shared_memory(handle, true, peer_handle());
#else
  base::SharedMemory shared_memory(handle, true);
#endif
  if (!shared_memory.Map(buffer_size)) {
    OnRemoveBlob(url);
    return;
  }

  BlobData::Item item;
  item.SetToDataExternal(static_cast<char*>(shared_memory.memory()),
                        buffer_size);
  blob_storage_context_->controller()->AppendBlobDataItem(url, item);
}

void FileAndBlobMessageFilter::OnFinishBuildingBlob(
    const GURL& url, const std::string& content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->FinishBuildingBlob(url, content_type);
}

void FileAndBlobMessageFilter::OnCloneBlob(
    const GURL& url, const GURL& src_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->CloneBlob(url, src_url);
  blob_urls_.insert(url.spec());
}

void FileAndBlobMessageFilter::OnRemoveBlob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->RemoveBlob(url);
  blob_urls_.erase(url.spec());
}

void FileAndBlobMessageFilter::DidFinish(int request_id,
                                         base::PlatformFileError result) {
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidSucceed(request_id));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  UnregisterOperation(request_id);
}

void FileAndBlobMessageFilter::DidCancel(int request_id,
                                         base::PlatformFileError result) {
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidSucceed(request_id));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  // For Cancel we do not create a new operation, so no unregister here.
}

void FileAndBlobMessageFilter::DidGetMetadata(
    int request_id,
    base::PlatformFileError result,
    const base::PlatformFileInfo& info,
    const FilePath& platform_path) {
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidReadMetadata(request_id, info, platform_path));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  UnregisterOperation(request_id);
}

void FileAndBlobMessageFilter::DidReadDirectory(
    int request_id,
    base::PlatformFileError result,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidReadDirectory(request_id, entries, has_more));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  UnregisterOperation(request_id);
}

void FileAndBlobMessageFilter::DidOpenFile(int request_id,
                                           base::PlatformFileError result,
                                           base::PlatformFile file,
                                           base::ProcessHandle peer_handle) {
  if (result == base::PLATFORM_FILE_OK) {
    IPC::PlatformFileForTransit file_for_transit =
        file != base::kInvalidPlatformFileValue ?
            IPC::GetFileHandleForProcess(file, peer_handle, true) :
            IPC::InvalidPlatformFileForTransit();
    Send(new FileSystemMsg_DidOpenFile(request_id, file_for_transit));
  } else {
    Send(new FileSystemMsg_DidFail(request_id, result));
  }
  UnregisterOperation(request_id);
}

void FileAndBlobMessageFilter::DidWrite(int request_id,
                                        base::PlatformFileError result,
                                        int64 bytes,
                                        bool complete) {
  if (result == base::PLATFORM_FILE_OK) {
    Send(new FileSystemMsg_DidWrite(request_id, bytes, complete));
    if (complete)
      UnregisterOperation(request_id);
  } else {
    Send(new FileSystemMsg_DidFail(request_id, result));
    UnregisterOperation(request_id);
  }
}

void FileAndBlobMessageFilter::DidOpenFileSystem(int request_id,
                                                 base::PlatformFileError result,
                                                 const std::string& name,
                                                 const GURL& root) {
  if (result == base::PLATFORM_FILE_OK) {
    DCHECK(root.is_valid());
    Send(new FileSystemMsg_DidOpenFileSystem(request_id, name, root));
  } else {
    Send(new FileSystemMsg_DidFail(request_id, result));
  }
  // For OpenFileSystem we do not create a new operation, so no unregister here.
}

void FileAndBlobMessageFilter::DidCreateSnapshot(
    int request_id,
    const GURL& blob_url,
    base::PlatformFileError result,
    const base::PlatformFileInfo& info,
    const FilePath& platform_path,
    const scoped_refptr<webkit_blob::DeletableFileReference>& unused) {
  if (result != base::PLATFORM_FILE_OK) {
    Send(new FileSystemMsg_DidFail(request_id, result));
    return;
  }

  FilePath::StringType extension = platform_path.Extension();
  if (!extension.empty())
    extension = extension.substr(1);  // Strip leading ".".

  // This may fail, but then we'll be just setting the empty mime type.
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(extension, &mime_type);

  // Register the created file to the blob registry.
  // Blob storage automatically finds and refs deletable files, so we don't
  // need to do anything for the returned file reference (|unused|) here.
  BlobData::Item item;
  item.SetToFile(platform_path, 0, -1, base::Time());
  BlobStorageController* controller = blob_storage_context_->controller();
  controller->StartBuildingBlob(blob_url);
  controller->AppendBlobDataItem(blob_url, item);
  controller->FinishBuildingBlob(blob_url, mime_type);
  blob_urls_.insert(blob_url.spec());

  // Return the file info and platform_path.
  Send(new FileSystemMsg_DidReadMetadata(request_id, info, platform_path));
}

FileSystemOperationInterface* FileAndBlobMessageFilter::GetNewOperation(
    const GURL& target_path,
    int request_id) {
  FileSystemOperationInterface* operation =
      context_->CreateFileSystemOperation(
          target_path,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  DCHECK(operation);
  operations_.AddWithID(operation, request_id);
  return operation;
}

void FileAndBlobMessageFilter::UnregisterOperation(int request_id) {
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}
