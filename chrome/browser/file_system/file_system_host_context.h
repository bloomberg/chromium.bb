// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_

#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_quota.h"

class GURL;

// This is owned by profile and shared by all the FileSystemDispatcherHost
// that shared by the same profile.
class FileSystemHostContext
    : public base::RefCountedThreadSafe<FileSystemHostContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  FileSystemHostContext(const FilePath& data_path, bool is_incognito);
  virtual ~FileSystemHostContext();

  // Quota related methods.
  bool CheckOriginQuota(const GURL& url, int64 growth);
  void SetOriginQuotaUnlimited(const GURL& url);
  void ResetOriginQuotaUnlimited(const GURL& url);

  fileapi::FileSystemPathManager* path_manager() { return path_manager_.get(); }

 private:
  scoped_ptr<fileapi::FileSystemQuota> quota_manager_;
  scoped_ptr<fileapi::FileSystemPathManager> path_manager_;

  bool allow_file_access_from_files_;
  bool unlimited_quota_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemHostContext);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_FILE_SYSTEM_HOST_CONTEXT_H_
