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

namespace base {
class Time;
}

class ChromeURLRequestContext;
class FilePath;
class GURL;
class HostContentSettingsMap;
class Profile;
class Receiver;
class RenderMessageFilter;
class URLRequestContextGetter;

namespace net {
class URLRequestContext;
}  // namespace net

namespace fileapi {
class FileSystemContext;
class FileSystemOperation;
}

class FileSystemDispatcherHost : public BrowserMessageFilter {
 public:
  // Used by the renderer.
  explicit FileSystemDispatcherHost(Profile* profile);
  // Used by the worker, since it has the context handy already.
  explicit FileSystemDispatcherHost(ChromeURLRequestContext* context);
  ~FileSystemDispatcherHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelConnected(int32 peer_pid);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  void UnregisterOperation(int request_id);

 private:
  void OnOpen(int request_id,
              const GURL& origin_url,
              fileapi::FileSystemType type,
              int64 requested_size,
              bool create);
  void OnMove(int request_id,
              const FilePath& src_path,
              const FilePath& dest_path);
  void OnCopy(int request_id,
              const FilePath& src_path,
              const FilePath& dest_path);
  void OnRemove(int request_id, const FilePath& path, bool recursive);
  void OnReadMetadata(int request_id, const FilePath& path);
  void OnCreate(int request_id,
                const FilePath& path,
                bool exclusive,
                bool is_directory,
                bool recursive);
  void OnExists(int request_id, const FilePath& path, bool is_directory);
  void OnReadDirectory(int request_id, const FilePath& path);
  void OnWrite(int request_id,
               const FilePath& path,
               const GURL& blob_url,
               int64 offset);
  void OnTruncate(int request_id, const FilePath& path, int64 length);
  void OnTouchFile(int request_id,
                   const FilePath& path,
                   const base::Time& last_access_time,
                   const base::Time& last_modified_time);
  void OnCancel(int request_id, int request_to_cancel);

  // Creates a new FileSystemOperation.
  fileapi::FileSystemOperation* GetNewOperation(int request_id);

  scoped_refptr<fileapi::FileSystemContext> context_;

  // Used to look up permissions.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Keeps ongoing file system operations.
  typedef IDMap<fileapi::FileSystemOperation> OperationsMap;
  OperationsMap operations_;

  // This holds the URLRequestContextGetter until Init() can be called from the
  // IO thread, which will extract the net::URLRequestContext from it.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
  scoped_refptr<net::URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcherHost);
};

#endif  // CONTENT_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
