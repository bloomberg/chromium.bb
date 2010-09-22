// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CALLBACK_DISPATCHER_H_
#define CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CALLBACK_DISPATCHER_H_

#include "webkit/fileapi/file_system_callback_dispatcher.h"

class FileSystemDispatcherHost;

class BrowserFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  BrowserFileSystemCallbackDispatcher(FileSystemDispatcherHost* dispatcher_host,
                                      int request_id);

  // FileSystemCallbackDispatcher implementation.
  virtual void DidSucceed();
  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info);
  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool has_more);
  virtual void DidOpenFileSystem(const string16& name,
                                 const FilePath& root_path);
  virtual void DidFail(base::PlatformFileError error_code);

 private:
  scoped_refptr<FileSystemDispatcherHost> dispatcher_host_;
  int request_id_;
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CALLBACK_DISPATCHER_H_
