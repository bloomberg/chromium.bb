// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_MESSAGE_FILTER_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/browser/browser_message_filter.h"
#include "ipc/ipc_platform_file.h"

namespace base {
struct PlatformFileInfo;
}

namespace IPC {
class Message;
}

class FileUtilitiesMessageFilter : public BrowserMessageFilter {
 public:
  explicit FileUtilitiesMessageFilter(int process_id);

  // BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(const IPC::Message& message,
                                        BrowserThread::ID* thread);
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);
 private:
  ~FileUtilitiesMessageFilter();

  typedef void (*FileInfoWriteFunc)(IPC::Message* reply_msg,
                                    const base::PlatformFileInfo& file_info);

  void OnGetFileSize(const FilePath& path, int64* result);
  void OnGetFileModificationTime(const FilePath& path, base::Time* result);
  void OnOpenFile(const FilePath& path,
                  int mode,
                  IPC::PlatformFileForTransit* result);

  // The ID of this process.
  int process_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilitiesMessageFilter);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_FILE_UTILITIES_MESSAGE_FILTER_H_
