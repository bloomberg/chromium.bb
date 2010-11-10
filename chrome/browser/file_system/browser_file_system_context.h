// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CONTEXT_H_
#define CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CONTEXT_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "webkit/fileapi/sandboxed_file_system_context.h"

class FilePath;
class GURL;

// This is owned by profile and shared by all the FileSystemDispatcherHost
// that shared by the same profile.  This class is just a thin wrapper around
// fileapi::SandboxedFileSystemContext.
class BrowserFileSystemContext
    : public base::RefCountedThreadSafe<BrowserFileSystemContext,
                                        BrowserThread::DeleteOnIOThread>,
      public fileapi::SandboxedFileSystemContext {
 public:
  BrowserFileSystemContext(const FilePath& profile_path, bool is_incognito);
  virtual ~BrowserFileSystemContext();

  // Quota related methods.
  void SetOriginQuotaUnlimited(const GURL& url);
  void ResetOriginQuotaUnlimited(const GURL& url);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserFileSystemContext);
};

#endif  // CHROME_BROWSER_FILE_SYSTEM_BROWSER_FILE_SYSTEM_CONTEXT_H_
