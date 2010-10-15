// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/id_map.h"
#include "base/nullable_string16.h"
#include "base/process.h"
#include "base/platform_file.h"
#include "base/scoped_callback_factory.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
class Time;
}

class ChromeURLRequestContext;
class FileSystemHostContext;
class GURL;
class HostContentSettingsMap;
class Profile;
class Receiver;
class ResourceMessageFilter;
class URLRequestContextGetter;

class FileSystemDispatcherHost
    : public base::RefCountedThreadSafe<FileSystemDispatcherHost> {
 public:
  // Used by the renderer.
  FileSystemDispatcherHost(IPC::Message::Sender* sender,
                           Profile* profile);
  // Used by the worker, since it has the context handy already.
  FileSystemDispatcherHost(IPC::Message::Sender* sender,
                           ChromeURLRequestContext* context);
  ~FileSystemDispatcherHost();
  void Init(base::ProcessHandle process_handle);
  void Shutdown();

  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

  void OnOpenFileSystem(int request_id,
                        const GURL& origin_url,
                        fileapi::FileSystemType type,
                        int64 requested_size);
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
  void Send(IPC::Message* message);
  void RemoveCompletedOperation(int request_id);

 private:
  // Creates a new FileSystemOperation.
  fileapi::FileSystemOperation* GetNewOperation(int request_id);

  // Checks the validity of a given |path| for reading.
  // Returns true if the given |path| is a valid FileSystem path.
  // Otherwise it sends back PLATFORM_FILE_ERROR_SECURITY to the
  // dispatcher and returns false.
  bool VerifyFileSystemPathForRead(const FilePath& path, int request_id);

  // Checks the validity of a given |path| for writing.
  // Returns true if the given |path| is a valid FileSystem path, and
  // its origin embedded in the path has the right to write as much as
  // the given |growth|.
  // Otherwise it sends back PLATFORM_FILE_ERROR_SECURITY if the path
  // is not valid for writing, or sends back PLATFORM_FILE_ERROR_NO_SPACE
  // if the origin is not allowed to increase the usage by |growth|.
  // If |create| flag is true this also checks if the |path| contains
  // any restricted names and chars. If it does, the call sends back
  // PLATFORM_FILE_ERROR_SECURITY to the dispatcher.
  bool VerifyFileSystemPathForWrite(const FilePath& path,
                                    int request_id,
                                    bool create,
                                    int64 growth);

  class OpenFileSystemTask;

  // Checks if a given |path| does not contain any restricted names/chars
  // for new files. Returns true if the given |path| is safe.
  // Otherwise it sends back a security error code to the dispatcher and
  // returns false.
  bool CheckIfFilePathIsSafe(const FilePath& path, int request_id);

  // The sender to be used for sending out IPC messages.
  IPC::Message::Sender* message_sender_;

  // The handle of this process.
  base::ProcessHandle process_handle_;

  bool shutdown_;

  scoped_refptr<FileSystemHostContext> context_;

  // Used to look up permissions.
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

  // Keeps ongoing file system operations.
  typedef IDMap<fileapi::FileSystemOperation, IDMapOwnPointer> OperationsMap;
  OperationsMap operations_;

  // This holds the URLRequestContextGetter until Init() can be called from the
  // IO thread, which will extract the URLRequestContext from it.
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
  scoped_refptr<URLRequestContext> request_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcherHost);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_HOST_H_
