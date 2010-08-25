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

namespace WebKit {
class WebFileSystemCallbacks;
}

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

  // TODO(kinuko): add more implementation.

 private:
  void DidSucceed(int32 callbacks_id);
  void DidFail(int32 callbacks_id, int code);
  // TODO(kinuko): add more callbacks.

  IDMap<WebKit::WebFileSystemCallbacks> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemDispatcher);
};

#endif  // CHROME_COMMON_FILE_SYSTEM_FILE_SYSTEM_DISPATCHER_H_
