// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CONTEXT_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"

class FilePath;
class GURL;

namespace fileapi {
class FileSystemPathManager;
class FileSystemQuotaManager;
}

// This is owned by profile and shared by all the FileSystemDispatcherHost
// that shared by the same profile.
class BrowserFileSystemContext
    : public base::RefCountedThreadSafe<BrowserFileSystemContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  BrowserFileSystemContext(const FilePath& data_path, bool is_incognito);
  virtual ~BrowserFileSystemContext();

  // Quota related methods.
  bool CheckOriginQuota(const GURL& url, int64 growth);
  void SetOriginQuotaUnlimited(const GURL& url);
  void ResetOriginQuotaUnlimited(const GURL& url);

  fileapi::FileSystemPathManager* path_manager() { return path_manager_.get(); }

 private:
  scoped_ptr<fileapi::FileSystemQuotaManager> quota_manager_;
  scoped_ptr<fileapi::FileSystemPathManager> path_manager_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserFileSystemContext);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CONTEXT_H_
