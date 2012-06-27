// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_

#include <string>

#include "base/basictypes.h"
#include "base/file_util_proxy.h"
#include "base/hash_tables.h"
#include "base/id_map.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"
#include "webkit/blob/blob_data.h"
#include "webkit/fileapi/file_system_types.h"

class ChromeBlobStorageContext;
class FilePath;
class GURL;

namespace base {
class Time;
}

namespace fileapi {
class FileSystemURL;
class FileSystemContext;
class FileSystemOperationInterface;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

namespace webkit_blob {
class ShareableFileReference;
}

class FileAPIMessageFilter : public content::BrowserMessageFilter {
 public:
  // Used by the renderer process host on the UI thread.
  FileAPIMessageFilter(
      int process_id,
      net::URLRequestContextGetter* request_context_getter,
      fileapi::FileSystemContext* file_system_context,
      ChromeBlobStorageContext* blob_storage_context);
  // Used by the worker process host on the IO thread.
  FileAPIMessageFilter(
      int process_id,
      net::URLRequestContext* request_context,
      fileapi::FileSystemContext* file_system_context,
      ChromeBlobStorageContext* blob_storage_context);

  // content::BrowserMessageFilter implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void UnregisterOperation(int request_id);

 protected:
  virtual ~FileAPIMessageFilter();

  virtual void BadMessageReceived() OVERRIDE;

 private:
  void OnOpen(int request_id,
              const GURL& origin_url,
              fileapi::FileSystemType type,
              int64 requested_size,
              bool create);
  void OnMove(int request_id,
              const GURL& src_path,
              const GURL& dest_path);
  void OnCopy(int request_id,
              const GURL& src_path,
              const GURL& dest_path);
  void OnRemove(int request_id, const GURL& path, bool recursive);
  void OnReadMetadata(int request_id, const GURL& path);
  void OnCreate(int request_id,
                const GURL& path,
                bool exclusive,
                bool is_directory,
                bool recursive);
  void OnExists(int request_id, const GURL& path, bool is_directory);
  void OnReadDirectory(int request_id, const GURL& path);
  void OnWrite(int request_id,
               const GURL& path,
               const GURL& blob_url,
               int64 offset);
  void OnTruncate(int request_id, const GURL& path, int64 length);
  void OnTouchFile(int request_id,
                   const GURL& path,
                   const base::Time& last_access_time,
                   const base::Time& last_modified_time);
  void OnCancel(int request_id, int request_to_cancel);
  void OnOpenFile(int request_id, const GURL& path, int file_flags);
  void OnWillUpdate(const GURL& path);
  void OnDidUpdate(const GURL& path, int64 delta);
  void OnSyncGetPlatformPath(const GURL& path,
                             FilePath* platform_path);
  void OnCreateSnapshotFile(int request_id,
                            const GURL& blob_url,
                            const GURL& path);

  void OnStartBuildingBlob(const GURL& url);
  void OnAppendBlobDataItem(const GURL& url,
                            const webkit_blob::BlobData::Item& item);
  void OnAppendSharedMemory(const GURL& url, base::SharedMemoryHandle handle,
                            size_t buffer_size);
  void OnFinishBuildingBlob(const GURL& url, const std::string& content_type);
  void OnCloneBlob(const GURL& url, const GURL& src_url);
  void OnRemoveBlob(const GURL& url);

  // Callback functions to be used when each file operation is finished.
  void DidFinish(int request_id, base::PlatformFileError result);
  void DidCancel(int request_id, base::PlatformFileError result);
  void DidGetMetadata(int request_id,
                      base::PlatformFileError result,
                      const base::PlatformFileInfo& info,
                      const FilePath& platform_path);
  void DidReadDirectory(int request_id,
                        base::PlatformFileError result,
                        const std::vector<base::FileUtilProxy::Entry>& entries,
                        bool has_more);
  void DidOpenFile(int request_id,
                   base::PlatformFileError result,
                   base::PlatformFile file,
                   base::ProcessHandle peer_handle);
  void DidWrite(int request_id,
                base::PlatformFileError result,
                int64 bytes,
                bool complete);
  void DidOpenFileSystem(int request_id,
                         base::PlatformFileError result,
                         const std::string& name,
                         const GURL& root);
  void DidCreateSnapshot(
      int request_id,
      const GURL& blob_url,
      base::PlatformFileError result,
      const base::PlatformFileInfo& info,
      const FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  // Checks renderer's access permissions for single file.
  bool HasPermissionsForFile(const fileapi::FileSystemURL& url,
                             int permissions,
                             base::PlatformFileError* error);

  // Creates a new FileSystemOperationInterface based on |target_url|.
  fileapi::FileSystemOperationInterface* GetNewOperation(
      const fileapi::FileSystemURL& target_url,
      int request_id);

  int process_id_;

  fileapi::FileSystemContext* context_;

  // Keeps ongoing file system operations.
  typedef IDMap<fileapi::FileSystemOperationInterface> OperationsMap;
  OperationsMap operations_;

  // The getter holds the context until Init() can be called from the
  // IO thread, which will extract the net::URLRequestContext from it.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  net::URLRequestContext* request_context_;

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Keep track of blob URLs registered in this process. Need to unregister
  // all of them when the renderer process dies.
  base::hash_set<std::string> blob_urls_;

  DISALLOW_COPY_AND_ASSIGN(FileAPIMessageFilter);
};

#endif  // CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_
