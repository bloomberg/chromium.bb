// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_FILE_UTILITIES_DISPATCHER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_FILE_UTILITIES_DISPATCHER_HOST_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/process.h"
#include "base/ref_counted.h"
#include "ipc/ipc_message.h"

namespace base {
struct PlatformFileInfo;
}

namespace IPC {
class Message;
}

class FileUtilitiesDispatcherHost
    : public base::RefCountedThreadSafe<FileUtilitiesDispatcherHost> {
 public:
  FileUtilitiesDispatcherHost(IPC::Message::Sender* sender, int process_id);
  void Init(base::ProcessHandle process_handle);
  void Shutdown();
  bool OnMessageReceived(const IPC::Message& message, bool* msg_is_ok);

  void Send(IPC::Message* message);

 private:
  friend class base::RefCountedThreadSafe<FileUtilitiesDispatcherHost>;
  ~FileUtilitiesDispatcherHost();

  typedef void (*FileInfoWriteFunc)(IPC::Message* reply_msg,
                                    const base::PlatformFileInfo& file_info);

  void OnGetFileSize(const FilePath& path, IPC::Message* reply_msg);
  void OnGetFileModificationTime(const FilePath& path, IPC::Message* reply_msg);
  void OnGetFileInfoOnFileThread(const FilePath& path,
                                 IPC::Message* reply_msg,
                                 FileInfoWriteFunc write_func);
  void OnOpenFile(const FilePath& path, int mode,IPC::Message* reply_msg);
  void OnOpenFileOnFileThread(const FilePath& path,
                              int mode,
                              IPC::Message* reply_msg);

  // The sender to be used for sending out IPC messages.
  IPC::Message::Sender* message_sender_;

  // The ID of this process.
  int process_id_;

  // The handle of this process.
  base::ProcessHandle process_handle_;

  bool shutdown_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilitiesDispatcherHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_FILE_UTILITIES_DISPATCHER_HOST_H_
