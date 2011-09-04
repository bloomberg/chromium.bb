// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_

#include <set>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "content/browser/browser_message_filter.h"
#include "webkit/fileapi/file_system_types.h"

class FilePath;
class GURL;
class Receiver;
class RenderMessageFilter;

namespace base {
class Time;
}

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;
}

namespace net {
class URLRequestContext;
class URLRequestContextGetter;
}  // namespace net

class FileSystemDispatcherHost : public BrowserMessageFilter {
 public:
  // Used by the renderer process host on the UI thread.
  FileSystemDispatcherHost(
      net::URLRequestContextGetter* request_context_getter,
      fileapi::FileSystemContext* file_system_context);
  // Used by the worker process host on the IO thread.
  FileSystemDispatcherHost(
      net::URLRequestContext* request_context,
      fileapi::FileSystemContext* file_system_context);
  virtual ~FileSystemDispatcherHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void UnregisterOperation(int request_id);

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

  // Creates a new FileSystemOperation.
  fileapi::FileSystemOperation* GetNewOperation(int request_id);

  fileapi::FileSystemContext* context_;

  // Keeps ongoing file system operations.
  typedef IDMap<fileapi::FileSystemOperation> OperationsMap;
  OperationsMap operations_;

  // The getter holds the context until Init() can be called from the
  // IO thread, which will extract the net::URLRequestContext from it.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  net::URLRequestContext* request_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcherHost);
};

#endif  // CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
