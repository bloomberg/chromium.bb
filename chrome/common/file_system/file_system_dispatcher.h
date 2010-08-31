// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
#define CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/nullable_string16.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"

namespace WebKit {
struct WebFileInfo;
class WebFileSystemCallbacks;
struct WebFileSystemEntry;
}

namespace file_util {
struct FileInfo;
}

struct ViewMsg_FileSystem_DidReadDirectory_Params;

// Dispatches and sends file system related messages sent to/from a child
// process from/to the main browser process.  There is one instance
// per child process.  Messages are dispatched on the main child thread.
class FileSystemDispatcher {
 public:
  FileSystemDispatcher();
  ~FileSystemDispatcher();

  bool OnMessageReceived(const IPC::Message& msg);

  void Move(
      const string16& src_path,
      const string16& dest_path,
      WebKit::WebFileSystemCallbacks* callbacks);
  void Copy(
      const string16& src_path,
      const string16& dest_path,
      WebKit::WebFileSystemCallbacks* callbacks);
  void Remove(
      const string16& path,
      WebKit::WebFileSystemCallbacks* callbacks);
  void ReadMetadata(
      const string16& path,
      WebKit::WebFileSystemCallbacks* callbacks);
  void Create(
      const string16& path,
      bool exclusive,
      bool for_directory,
      WebKit::WebFileSystemCallbacks* callbacks);
  void Exists(
      const string16& path,
      bool for_directory,
      WebKit::WebFileSystemCallbacks* callbacks);
  void ReadDirectory(
      const string16& path,
      WebKit::WebFileSystemCallbacks* callbacks);

 private:
  void DidSucceed(int request_id);
  void DidReadMetadata(
      int request_id,
      const file_util::FileInfo& file_info);
  void DidReadDirectory(
      const ViewMsg_FileSystem_DidReadDirectory_Params& params);
  void DidFail(
      int request_id,
      WebKit::WebFileError);

  IDMap<WebKit::WebFileSystemCallbacks> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcher);
};

#endif  // CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
