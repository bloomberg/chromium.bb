// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_NULL_FILE_SYSTEM_CALLBACK_DISPATCHER_H_
#define CONTENT_RENDERER_PEPPER_NULL_FILE_SYSTEM_CALLBACK_DISPATCHER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/quota/quota_types.h"

class GURL;

namespace base {
class FilePath;
class FileUtilProxy;
struct PlatformFileInfo;
}  // namespace base

namespace content {

class NullFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  NullFileSystemCallbackDispatcher() {}

  virtual ~NullFileSystemCallbackDispatcher(){}

  virtual void DidSucceed() OVERRIDE;
  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info,
                               const base::FilePath& platform_path) OVERRIDE;
  virtual void DidCreateSnapshotFile(
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path) OVERRIDE;
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE;
  virtual void DidOpenFileSystem(const std::string& name,
                                 const GURL& root) OVERRIDE;
  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE;
  virtual void DidOpenFile(base::PlatformFile file,
                           int file_open_id,
                           quota::QuotaLimitType quota_policy) OVERRIDE;
  virtual void DidFail(base::PlatformFileError platform_error) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullFileSystemCallbackDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_NULL_FILE_SYSTEM_CALLBACK_DISPATCHER_H_
