// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/fileapi_message_filter.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/common/fileapi/file_system_messages.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_platform_file.h"
#include "net/base/mime_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_permission_policy.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

using fileapi::FileSystemFileUtil;
using fileapi::FileSystemMountPointProvider;
using fileapi::FileSystemOperation;
using fileapi::FileSystemURL;
using fileapi::FileUpdateObserver;
using fileapi::LocalFileSystemOperation;
using fileapi::UpdateObserverList;
using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;

namespace content {

namespace {

void RevokeFilePermission(int child_id, const FilePath& path) {
  ChildProcessSecurityPolicyImpl::GetInstance()->RevokeAllPermissionsForFile(
    child_id, path);
}

}  // namespace

FileAPIMessageFilter::FileAPIMessageFilter(
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

FileAPIMessageFilter::FileAPIMessageFilter(
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

void FileAPIMessageFilter::OnChannelConnected(int32 peer_pid) {
  BrowserMessageFilter::OnChannelConnected(peer_pid);

  if (request_context_getter_.get()) {
    DCHECK(!request_context_);
    request_context_ = request_context_getter_->GetURLRequestContext();
    request_context_getter_ = NULL;
    DCHECK(request_context_);
  }
}

void FileAPIMessageFilter::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Unregister all the blob URLs that are previously registered in this
  // process.
  for (base::hash_set<std::string>::const_iterator iter = blob_urls_.begin();
       iter != blob_urls_.end(); ++iter) {
    blob_storage_context_->controller()->RemoveBlob(GURL(*iter));
  }

  // Close all files that are previously OpenFile()'ed in this process.
  if (!open_filesystem_urls_.empty()) {
    DLOG(INFO)
        << "File API: Renderer process shut down before NotifyCloseFile"
        << " for " << open_filesystem_urls_.size() << " files opened in PPAPI";
  }
  for (std::multiset<GURL>::const_iterator iter =
       open_filesystem_urls_.begin();
       iter != open_filesystem_urls_.end(); ++iter) {
    FileSystemURL url(context_->CrackURL(*iter));
    FileSystemOperation* operation = context_->CreateFileSystemOperation(
        url, NULL);
    if (operation)
      operation->NotifyCloseFile(url);
  }
}

void FileAPIMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == FileSystemHostMsg_SyncGetPlatformPath::ID)
    *thread = BrowserThread::FILE;
}

bool FileAPIMessageFilter::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(FileAPIMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_Open, OnOpen)
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_DeleteFileSystem, OnDeleteFileSystem)
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
    IPC_MESSAGE_HANDLER(FileSystemHostMsg_NotifyCloseFile, OnNotifyCloseFile)
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

void FileAPIMessageFilter::UnregisterOperation(int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(operations_.Lookup(request_id));
  operations_.Remove(request_id);
}

FileAPIMessageFilter::~FileAPIMessageFilter() {}

void FileAPIMessageFilter::BadMessageReceived() {
  RecordAction(UserMetricsAction("BadMessageTerminate_FAMF"));
  BrowserMessageFilter::BadMessageReceived();
}

void FileAPIMessageFilter::OnOpen(
    int request_id, const GURL& origin_url, fileapi::FileSystemType type,
    int64 requested_size, bool create) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (type == fileapi::kFileSystemTypeTemporary) {
    RecordAction(UserMetricsAction("OpenFileSystemTemporary"));
  } else if (type == fileapi::kFileSystemTypePersistent) {
    RecordAction(UserMetricsAction("OpenFileSystemPersistent"));
  }
  context_->OpenFileSystem(origin_url, type, create, base::Bind(
      &FileAPIMessageFilter::DidOpenFileSystem, this, request_id));
}

void FileAPIMessageFilter::OnDeleteFileSystem(
    int request_id,
    const GURL& origin_url,
    fileapi::FileSystemType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context_->DeleteFileSystem(origin_url, type, base::Bind(
      &FileAPIMessageFilter::DidDeleteFileSystem, this, request_id));
}

void FileAPIMessageFilter::OnMove(
    int request_id, const GURL& src_path, const GURL& dest_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL src_url(context_->CrackURL(src_path));
  FileSystemURL dest_url(context_->CrackURL(dest_path));
  const int src_permissions =
      fileapi::kReadFilePermissions | fileapi::kWriteFilePermissions;
  if (!HasPermissionsForFile(src_url, src_permissions, &error) ||
      !HasPermissionsForFile(
          dest_url, fileapi::kCreateFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(dest_url, request_id);
  if (!operation)
    return;
  operation->Move(
      src_url, dest_url,
      base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
}

void FileAPIMessageFilter::OnCopy(
    int request_id, const GURL& src_path, const GURL& dest_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL src_url(context_->CrackURL(src_path));
  FileSystemURL dest_url(context_->CrackURL(dest_path));
  if (!HasPermissionsForFile(src_url, fileapi::kReadFilePermissions, &error) ||
      !HasPermissionsForFile(
          dest_url, fileapi::kCreateFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(dest_url, request_id);
  if (!operation)
    return;
  operation->Copy(
      src_url, dest_url,
      base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
}

void FileAPIMessageFilter::OnRemove(
    int request_id, const GURL& path, bool recursive) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, fileapi::kWriteFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->Remove(
      url, recursive,
      base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
}

void FileAPIMessageFilter::OnReadMetadata(
    int request_id, const GURL& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, fileapi::kReadFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->GetMetadata(
      url,
      base::Bind(&FileAPIMessageFilter::DidGetMetadata, this, request_id));
}

void FileAPIMessageFilter::OnCreate(
    int request_id, const GURL& path, bool exclusive,
    bool is_directory, bool recursive) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, fileapi::kCreateFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  if (is_directory) {
    operation->CreateDirectory(
        url, exclusive, recursive,
        base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
  } else {
    operation->CreateFile(
        url, exclusive,
        base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
  }
}

void FileAPIMessageFilter::OnExists(
    int request_id, const GURL& path, bool is_directory) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, fileapi::kReadFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  if (is_directory) {
    operation->DirectoryExists(
        url,
        base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
  } else {
    operation->FileExists(
        url,
        base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
  }
}

void FileAPIMessageFilter::OnReadDirectory(
    int request_id, const GURL& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, fileapi::kReadFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->ReadDirectory(
      url, base::Bind(&FileAPIMessageFilter::DidReadDirectory,
                       this, request_id));
}

void FileAPIMessageFilter::OnWrite(
    int request_id,
    const GURL& path,
    const GURL& blob_url,
    int64 offset) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!request_context_) {
    // We can't write w/o a request context, trying to do so will crash.
    NOTREACHED();
    return;
  }

  FileSystemURL url(context_->CrackURL(path));
  base::PlatformFileError error;
  if (!HasPermissionsForFile(url, fileapi::kWriteFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->Write(
      request_context_, url, blob_url, offset,
      base::Bind(&FileAPIMessageFilter::DidWrite, this, request_id));
}

void FileAPIMessageFilter::OnTruncate(
    int request_id,
    const GURL& path,
    int64 length) {
  base::PlatformFileError error;
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, fileapi::kWriteFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->Truncate(
      url, length,
      base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
}

void FileAPIMessageFilter::OnTouchFile(
    int request_id,
    const GURL& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FileSystemURL url(context_->CrackURL(path));
  base::PlatformFileError error;
  if (!HasPermissionsForFile(url, fileapi::kCreateFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->TouchFile(
      url, last_access_time, last_modified_time,
      base::Bind(&FileAPIMessageFilter::DidFinish, this, request_id));
}

void FileAPIMessageFilter::OnCancel(
    int request_id,
    int request_id_to_cancel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FileSystemOperation* write = operations_.Lookup(request_id_to_cancel);
  if (write) {
    // The cancel will eventually send both the write failure and the cancel
    // success.
    write->Cancel(
        base::Bind(&FileAPIMessageFilter::DidCancel, this, request_id));
  } else {
    // The write already finished; report that we failed to stop it.
    Send(new FileSystemMsg_DidFail(
        request_id, base::PLATFORM_FILE_ERROR_INVALID_OPERATION));
  }
}

void FileAPIMessageFilter::OnOpenFile(
    int request_id, const GURL& path, int file_flags) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  base::PlatformFileError error;
  const int open_permissions = base::PLATFORM_FILE_OPEN |
                               (file_flags & fileapi::kOpenFilePermissions);
  FileSystemURL url(context_->CrackURL(path));
  if (!HasPermissionsForFile(url, open_permissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->OpenFile(
      url, file_flags, peer_handle(),
      base::Bind(&FileAPIMessageFilter::DidOpenFile, this, request_id, path));
}

void FileAPIMessageFilter::OnNotifyCloseFile(const GURL& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Remove |path| from the set of opened urls. It must only be called for a URL
  // that is successfully opened and enrolled in DidOpenFile.
  std::multiset<GURL>::iterator iter = open_filesystem_urls_.find(path);
  DCHECK(iter != open_filesystem_urls_.end());
  open_filesystem_urls_.erase(iter);

  FileSystemURL url(context_->CrackURL(path));

  // Do not use GetNewOperation() here, because NotifyCloseFile is a one-way
  // operation that does not have request_id by which we respond back.
  FileSystemOperation* operation = context_->CreateFileSystemOperation(
      url, NULL);
  if (operation)
    operation->NotifyCloseFile(url);
}

void FileAPIMessageFilter::OnWillUpdate(const GURL& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FileSystemURL url(context_->CrackURL(path));
  if (!url.is_valid())
    return;
  const UpdateObserverList* observers =
      context_->GetUpdateObservers(url.type());
  if (!observers)
    return;
  observers->Notify(&FileUpdateObserver::OnStartUpdate, MakeTuple(url));
}

void FileAPIMessageFilter::OnDidUpdate(const GURL& path, int64 delta) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FileSystemURL url(context_->CrackURL(path));
  if (!url.is_valid())
    return;
  const UpdateObserverList* observers =
      context_->GetUpdateObservers(url.type());
  if (!observers)
    return;
  observers->Notify(&FileUpdateObserver::OnUpdate, MakeTuple(url, delta));
  observers->Notify(&FileUpdateObserver::OnEndUpdate, MakeTuple(url));
}

void FileAPIMessageFilter::OnSyncGetPlatformPath(
    const GURL& path, FilePath* platform_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(platform_path);
  *platform_path = FilePath();
  FileSystemURL url(context_->CrackURL(path));
  if (!url.is_valid())
    return;

  // Make sure if this file is ok to be read (in the current architecture
  // which means roughly same as the renderer is allowed to get the platform
  // path to the file).
  base::PlatformFileError error;
  if (!HasPermissionsForFile(url, fileapi::kReadFilePermissions, &error))
    return;

  // This is called only by pepper plugin as of writing to get the
  // underlying platform path to upload a file in the sandboxed filesystem
  // (e.g. TEMPORARY or PERSISTENT).
  // TODO(kinuko): this hack should go away once appropriate upload-stream
  // handling based on element types is supported.
  LocalFileSystemOperation* operation =
      context_->CreateFileSystemOperation(
          url, NULL)->AsLocalFileSystemOperation();
  DCHECK(operation);
  if (!operation)
    return;

  operation->SyncGetPlatformPath(url, platform_path);

  // The path is to be attached to URLLoader so we grant read permission
  // for the file. (We first need to check if it can already be read not to
  // overwrite existing permissions)
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          process_id_, *platform_path)) {
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        process_id_, *platform_path);
  }
}

void FileAPIMessageFilter::OnCreateSnapshotFile(
    int request_id, const GURL& blob_url, const GURL& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  FileSystemURL url(context_->CrackURL(path));
  base::Callback<void(const FilePath&)> register_file_callback =
      base::Bind(&FileAPIMessageFilter::RegisterFileAsBlob,
                 this, blob_url, url);

  // Make sure if this file can be read by the renderer as this is
  // called when the renderer is about to create a new File object
  // (for reading the file).
  base::PlatformFileError error;
  if (!HasPermissionsForFile(url, fileapi::kReadFilePermissions, &error)) {
    Send(new FileSystemMsg_DidFail(request_id, error));
    return;
  }

  FileSystemOperation* operation = GetNewOperation(url, request_id);
  if (!operation)
    return;
  operation->CreateSnapshotFile(
      url,
      base::Bind(&FileAPIMessageFilter::DidCreateSnapshot,
                 this, request_id, register_file_callback));
}

void FileAPIMessageFilter::OnStartBuildingBlob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->StartBuildingBlob(url);
  blob_urls_.insert(url.spec());
}

void FileAPIMessageFilter::OnAppendBlobDataItem(
    const GURL& url, const BlobData::Item& item) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (item.type() == BlobData::Item::TYPE_FILE_FILESYSTEM) {
    // TODO(kinuko): Implement permission check for filesystem files.
    // http://crbug.com/169423
    OnRemoveBlob(url);
    return;
  }
  if (item.type() == BlobData::Item::TYPE_FILE &&
      !ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          process_id_, item.path())) {
    OnRemoveBlob(url);
    return;
  }
  if (item.length() == 0) {
    BadMessageReceived();
    return;
  }
  blob_storage_context_->controller()->AppendBlobDataItem(url, item);
}

void FileAPIMessageFilter::OnAppendSharedMemory(
    const GURL& url, base::SharedMemoryHandle handle, size_t buffer_size) {
  DCHECK(base::SharedMemory::IsHandleValid(handle));
  if (!buffer_size) {
    BadMessageReceived();
    return;
  }
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
  item.SetToSharedBytes(static_cast<char*>(shared_memory.memory()),
                        buffer_size);
  blob_storage_context_->controller()->AppendBlobDataItem(url, item);
}

void FileAPIMessageFilter::OnFinishBuildingBlob(
    const GURL& url, const std::string& content_type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->FinishBuildingBlob(url, content_type);
}

void FileAPIMessageFilter::OnCloneBlob(
    const GURL& url, const GURL& src_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->CloneBlob(url, src_url);
  blob_urls_.insert(url.spec());
}

void FileAPIMessageFilter::OnRemoveBlob(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  blob_storage_context_->controller()->RemoveBlob(url);
  blob_urls_.erase(url.spec());
}

void FileAPIMessageFilter::DidFinish(int request_id,
                                     base::PlatformFileError result) {
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidSucceed(request_id));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  UnregisterOperation(request_id);
}

void FileAPIMessageFilter::DidCancel(int request_id,
                                     base::PlatformFileError result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidSucceed(request_id));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  // For Cancel we do not create a new operation, so no unregister here.
}

void FileAPIMessageFilter::DidGetMetadata(
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

void FileAPIMessageFilter::DidReadDirectory(
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

void FileAPIMessageFilter::DidOpenFile(int request_id,
                                       const GURL& path,
                                       base::PlatformFileError result,
                                       base::PlatformFile file,
                                       base::ProcessHandle peer_handle) {
  if (result == base::PLATFORM_FILE_OK) {
    IPC::PlatformFileForTransit file_for_transit =
        file != base::kInvalidPlatformFileValue ?
            IPC::GetFileHandleForProcess(file, peer_handle, true) :
            IPC::InvalidPlatformFileForTransit();
    open_filesystem_urls_.insert(path);
    Send(new FileSystemMsg_DidOpenFile(request_id, file_for_transit));
  } else {
    Send(new FileSystemMsg_DidFail(request_id, result));
  }
  UnregisterOperation(request_id);
}

void FileAPIMessageFilter::DidWrite(int request_id,
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

void FileAPIMessageFilter::DidOpenFileSystem(int request_id,
                                             base::PlatformFileError result,
                                             const std::string& name,
                                             const GURL& root) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result == base::PLATFORM_FILE_OK) {
    DCHECK(root.is_valid());
    Send(new FileSystemMsg_DidOpenFileSystem(request_id, name, root));
  } else {
    Send(new FileSystemMsg_DidFail(request_id, result));
  }
  // For OpenFileSystem we do not create a new operation, so no unregister here.
}

void FileAPIMessageFilter::DidDeleteFileSystem(
    int request_id,
    base::PlatformFileError result) {
  if (result == base::PLATFORM_FILE_OK)
    Send(new FileSystemMsg_DidSucceed(request_id));
  else
    Send(new FileSystemMsg_DidFail(request_id, result));
  // For DeleteFileSystem we do not create a new operation,
  // so no unregister here.
}

void FileAPIMessageFilter::DidCreateSnapshot(
    int request_id,
    const base::Callback<void(const FilePath&)>& register_file_callback,
    base::PlatformFileError result,
    const base::PlatformFileInfo& info,
    const FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& unused) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (result != base::PLATFORM_FILE_OK) {
    Send(new FileSystemMsg_DidFail(request_id, result));
    return;
  }

  // Register the created file to the blob registry by calling
  // RegisterFileAsBlob.
  // Blob storage automatically finds and refs the file_ref, so we don't
  // need to do anything for the returned file reference (|unused|) here.
  register_file_callback.Run(platform_path);

  // Return the file info and platform_path.
  Send(new FileSystemMsg_DidReadMetadata(request_id, info, platform_path));
}

void FileAPIMessageFilter::RegisterFileAsBlob(const GURL& blob_url,
                                              const FileSystemURL& url,
                                              const FilePath& platform_path) {
  // Use the virtual path's extension to determine MIME type.
  FilePath::StringType extension = url.path().Extension();
  if (!extension.empty())
    extension = extension.substr(1);  // Strip leading ".".

  scoped_refptr<webkit_blob::ShareableFileReference> shareable_file =
      webkit_blob::ShareableFileReference::Get(platform_path);
  if (!ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
          process_id_, platform_path)) {
    // In order for the renderer to be able to read the file, it must be granted
    // read permission for the file's platform path. By now, it has already been
    // verified that the renderer has sufficient permissions to read the file.
    // It is still possible that ChildProcessSecurityPolicyImpl doesn't reflect
    // that the renderer can read the file's platform path. If this is the case
    // the renderer should be granted read permission for the file's platform
    // path. This can happen in the following situations:
    // - the file comes from sandboxed filesystem. Reading sandboxed files is
    //   always permitted, but only implicitly.
    // - the underlying filesystem returned newly created snapshot file.
    // - the file comes from an external drive filesystem. The renderer has
    //   already been granted read permission for the file's nominal path, but
    //   for drive files, platform paths differ from the nominal paths.
    DCHECK(shareable_file ||
           fileapi::SandboxMountPointProvider::CanHandleType(url.type()) ||
           url.type() == fileapi::kFileSystemTypeDrive);
    ChildProcessSecurityPolicyImpl::GetInstance()->GrantReadFile(
        process_id_, platform_path);
    if (shareable_file) {
      // This will revoke all permissions for the file when the last ref
      // of the file is dropped (assuming it's ok).
      shareable_file->AddFinalReleaseCallback(
          base::Bind(&RevokeFilePermission, process_id_));
    }
  }

  // This may fail, but then we'll be just setting the empty mime type.
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(extension, &mime_type);
  BlobData::Item item;
  item.SetToFilePathRange(platform_path, 0, -1, base::Time());
  BlobStorageController* controller = blob_storage_context_->controller();
  controller->StartBuildingBlob(blob_url);
  controller->AppendBlobDataItem(blob_url, item);
  controller->FinishBuildingBlob(blob_url, mime_type);
  blob_urls_.insert(blob_url.spec());
}

bool FileAPIMessageFilter::HasPermissionsForFile(
    const FileSystemURL& url, int permissions, base::PlatformFileError* error) {
  DCHECK(error);
  *error = base::PLATFORM_FILE_OK;

  if (!url.is_valid()) {
    *error = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return false;
  }

  FileSystemMountPointProvider* mount_point_provider =
      context_->GetMountPointProvider(url.type());
  if (!mount_point_provider) {
    *error = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return false;
  }

  FilePath file_path;
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  switch (mount_point_provider->GetPermissionPolicy(url, permissions)) {
    case fileapi::FILE_PERMISSION_ALWAYS_DENY:
      *error = base::PLATFORM_FILE_ERROR_SECURITY;
      return false;
    case fileapi::FILE_PERMISSION_ALWAYS_ALLOW:
      CHECK(mount_point_provider == context_->sandbox_provider());
      return true;
    case fileapi::FILE_PERMISSION_USE_FILE_PERMISSION: {
      const bool success = policy->HasPermissionsForFile(
          process_id_, url.path(), permissions);
      if (!success)
        *error = base::PLATFORM_FILE_ERROR_SECURITY;
      return success;
    }
    case fileapi::FILE_PERMISSION_USE_FILESYSTEM_PERMISSION: {
      const bool success = policy->HasPermissionsForFileSystem(
          process_id_, url.filesystem_id(), permissions);
      if (!success)
        *error = base::PLATFORM_FILE_ERROR_SECURITY;
      return success;
    }
  }
  NOTREACHED();
  *error = base::PLATFORM_FILE_ERROR_SECURITY;
  return false;
}

FileSystemOperation* FileAPIMessageFilter::GetNewOperation(
    const FileSystemURL& target_url,
    int request_id) {
  base::PlatformFileError error_code;
  FileSystemOperation* operation =
      context_->CreateFileSystemOperation(target_url, &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    Send(new FileSystemMsg_DidFail(request_id, error_code));
    return NULL;
  }

  DCHECK(operation);
  operations_.AddWithID(operation, request_id);
  return operation;
}

}  // namespace content
