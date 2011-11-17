// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
#define CONTENT_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_util_proxy.h"
#include "base/id_map.h"
#include "base/process.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
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
  virtual ~FileSystemDispatcher();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  bool OpenFileSystem(const GURL& origin_url,
                      fileapi::FileSystemType type,
                      long long size,
                      bool create,
                      fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Move(const GURL& src_path,
            const GURL& dest_path,
            fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Copy(const GURL& src_path,
            const GURL& dest_path,
            fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Remove(const GURL& path,
              bool recursive,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool ReadMetadata(const GURL& path,
                    fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Create(const GURL& path,
              bool exclusive,
              bool is_directory,
              bool recursive,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Exists(const GURL& path,
              bool for_directory,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool ReadDirectory(const GURL& path,
                     fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Truncate(const GURL& path,
                int64 offset,
                int* request_id_out,
                fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Write(const GURL& path,
             const GURL& blob_url,
             int64 offset,
             int* request_id_out,
             fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool Cancel(int request_id_to_cancel,
              fileapi::FileSystemCallbackDispatcher* dispatcher);
  bool TouchFile(const GURL& file_path,
                 const base::Time& last_access_time,
                 const base::Time& last_modified_time,
                 fileapi::FileSystemCallbackDispatcher* dispatcher);

  // This returns a raw open PlatformFile, unlike the above, which are
  // self-contained operations.
  bool OpenFile(const GURL& file_path,
                int file_flags,  // passed to FileUtilProxy::CreateOrOpen
                fileapi::FileSystemCallbackDispatcher* dispatcher);
 private:
  // Message handlers.
  void OnOpenComplete(
      int request_id,
      bool accepted,
      const std::string& name,
      const GURL& root);
  void OnDidSucceed(int request_id);
  void OnDidReadMetadata(int request_id,
                         const base::PlatformFileInfo& file_info,
                         const FilePath& platform_path);
  void OnDidReadDirectory(
      int request_id,
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more);
  void OnDidFail(int request_id, base::PlatformFileError error_code);
  void OnDidWrite(int request_id, int64 bytes, bool complete);
  void OnDidOpenFile(
      int request_id,
      IPC::PlatformFileForTransit file);

  IDMap<fileapi::FileSystemCallbackDispatcher, IDMapOwnPointer> dispatchers_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcher);
};

#endif  // CONTENT_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
