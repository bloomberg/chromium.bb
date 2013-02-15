// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/files/file_util_proxy.h"
#include "base/hash_tables.h"
#include "base/id_map.h"
#include "base/platform_file.h"
#include "base/shared_memory.h"
#include "content/public/browser/browser_message_filter.h"
#include "webkit/blob/blob_data.h"
#include "webkit/fileapi/file_system_types.h"

class GURL;

namespace base {
class FilePath;
class Time;
}

namespace fileapi {
class FileSystemURL;
class FileSystemContext;
class FileSystemOperation;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

namespace webkit_blob {
class ShareableFileReference;
}

namespace content {
class ChromeBlobStorageContext;

class FileAPIMessageFilter : public BrowserMessageFilter {
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

  // BrowserMessageFilter implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      BrowserThread::ID* thread) OVERRIDE;
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
  void OnDeleteFileSystem(int request_id,
                          const GURL& origin_url,
                          fileapi::FileSystemType type);
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
  void OnNotifyCloseFile(const GURL& path);
  void OnWillUpdate(const GURL& path);
  void OnDidUpdate(const GURL& path, int64 delta);
  void OnSyncGetPlatformPath(const GURL& path,
                             base::FilePath* platform_path);
  void OnCreateSnapshotFile(int request_id,
                            const GURL& path);
  void OnDidReceiveSnapshotFile(int request_id);

  void OnCreateSnapshotFile_Deprecated(int request_id,
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
                      const base::FilePath& platform_path);
  void DidReadDirectory(int request_id,
                        base::PlatformFileError result,
                        const std::vector<base::FileUtilProxy::Entry>& entries,
                        bool has_more);
  void DidOpenFile(int request_id,
                   const GURL& path,
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
  void DidDeleteFileSystem(int request_id,
                           base::PlatformFileError result);
  void DidCreateSnapshot(
      int request_id,
      const fileapi::FileSystemURL& url,
      base::PlatformFileError result,
      const base::PlatformFileInfo& info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  void DidCreateSnapshot_Deprecated(
      int request_id,
      const base::Callback<void(const base::FilePath&)>& register_file_callback,
      base::PlatformFileError result,
      const base::PlatformFileInfo& info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);
  // Registers the given file pointed by |virtual_path| and backed by
  // |platform_path| as the |blob_url|.  Called by DidCreateSnapshot_Deprecated.
  void RegisterFileAsBlob(const GURL& blob_url,
                          const fileapi::FileSystemURL& url,
                          const base::FilePath& platform_path);

  // Checks renderer's access permissions for single file.
  bool HasPermissionsForFile(const fileapi::FileSystemURL& url,
                             int permissions,
                             base::PlatformFileError* error);

  // Creates a new FileSystemOperation based on |target_url|.
  fileapi::FileSystemOperation* GetNewOperation(
      const fileapi::FileSystemURL& target_url,
      int request_id);

  int process_id_;

  fileapi::FileSystemContext* context_;

  // Keeps ongoing file system operations.
  typedef IDMap<fileapi::FileSystemOperation> OperationsMap;
  OperationsMap operations_;

  // The getter holds the context until Init() can be called from the
  // IO thread, which will extract the net::URLRequestContext from it.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  net::URLRequestContext* request_context_;

  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Keep track of blob URLs registered in this process. Need to unregister
  // all of them when the renderer process dies.
  base::hash_set<std::string> blob_urls_;

  // Used to keep snapshot files alive while a DidCreateSnapshot
  // is being sent to the renderer.
  std::map<int, scoped_refptr<webkit_blob::ShareableFileReference> >
      in_transit_snapshot_files_;

  // Keep track of file system file URLs opened by OpenFile() in this process.
  // Need to close all of them when the renderer process dies.
  std::multiset<GURL> open_filesystem_urls_;

  DISALLOW_COPY_AND_ASSIGN(FileAPIMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILEAPI_FILEAPI_MESSAGE_FILTER_H_
