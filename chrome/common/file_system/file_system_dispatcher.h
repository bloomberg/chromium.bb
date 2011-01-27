// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
#define CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
struct PlatformFileInfo;
}

class FilePath;
class GURL;

// Dispatches and sends file system related messages sent to/from a child
// process from/to the main browser process.  There is one instance
// per child process.  Messages are dispatched on the main child thread.
class FileSystemDispatcher : public IPC::Channel::Listener {
 public:
  FileSystemDispatcher();
  ~FileSystemDispatcher();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  bool OpenFileSystem(const GURL& origin_url,
                      fileapi::FileSystemType type,
                      long long size,
                      bool create,
                      fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Move(const FilePath& src_path,
            const FilePath& dest_path,
            fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Copy(const FilePath& src_path,
            const FilePath& dest_path,
            fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Remove(const FilePath& path,
              bool recursive,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool ReadMetadata(const FilePath& path,
                    fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Create(const FilePath& path,
              bool exclusive,
              bool is_directory,
              bool recursive,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Exists(const FilePath& path,
              bool for_directory,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool ReadDirectory(const FilePath& path,
                     fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Truncate(const FilePath& path,
                int64 offset,
                int* request_id_out,
                fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Write(const FilePath& path,
             const GURL& blob_url,
             int64 offset,
             int* request_id_out,
             fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Cancel(int request_id_to_cancel,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool TouchFile(const FilePath& file_path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time,
                 fileapi::FileSystemCallbackDispatcher* dispatcher);

 private:
  // Message handler for OpenFileSystem.
  void OnOpenFileSystemRequestComplete(
      int request_id,
      bool accepted,
      const std::string& name,
      const FilePath& root_path);

  // Message handlers for regular file system operations.
  void DidSucceed(int request_id);
  void DidReadMetadata(int request_id,
                       const base::PlatformFileInfo& file_info);
  void DidReadDirectory(
      int request_id,
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more);
  void DidFail(int request_id, base::PlatformFileError error_code);
  void DidWrite(int request_id, int64 bytes, bool complete);

  IDMap<fileapi::FileSystemCallbackDispatcher, IDMapOwnPointer> dispatchers_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcher);
};

#endif  // CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
