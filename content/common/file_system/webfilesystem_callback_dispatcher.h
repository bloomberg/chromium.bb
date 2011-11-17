// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILE_SYSTEM_WEBFILESYSTEM_CALLBACK_DISPATCHER_H_
#define CONTENT_COMMON_FILE_SYSTEM_WEBFILESYSTEM_CALLBACK_DISPATCHER_H_

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

class GURL;

namespace WebKit {
class WebFileSystemCallbacks;
}

class WebFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit WebFileSystemCallbackDispatcher(
      WebKit::WebFileSystemCallbacks* callbacks);

  // FileSystemCallbackDispatcher implementation
  virtual void DidSucceed() OVERRIDE;
  virtual void DidReadMetadata(
      const base::PlatformFileInfo& file_info,
      const FilePath& platform_path) OVERRIDE;
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE;
  virtual void DidOpenFileSystem(const std::string&,
                                 const GURL&) OVERRIDE;
  virtual void DidFail(base::PlatformFileError) OVERRIDE;
  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE;

 private:
  WebKit::WebFileSystemCallbacks* callbacks_;
};

#endif  // CONTENT_COMMON_FILE_SYSTEM_WEBFILESYSTEM_CALLBACK_DISPATCHER_H_
