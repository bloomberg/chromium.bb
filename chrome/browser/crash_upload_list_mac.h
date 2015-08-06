// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_MAC_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_MAC_H_

#include "base/macros.h"
#include "components/upload_list/crash_upload_list.h"

namespace base {
class FilePath;
class SequencedWorkerPool;
}

// A CrashUploadList that retrieves the list of uploaded reports from the
// Crashpad database.
class CrashUploadListMac : public CrashUploadList {
 public:
  // The |upload_log_path| argument is unused. It is only accepted because the
  // base class constructor requires it, although it is entirely unused with
  // LoadUploadList() being overridden.
  CrashUploadListMac(
      Delegate* delegate,
      const base::FilePath& upload_log_path,
      const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

 protected:
  ~CrashUploadListMac() override;

  // Called on a blocking pool thread.
  void LoadUploadList() override;

  DISALLOW_COPY_AND_ASSIGN(CrashUploadListMac);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_MAC_H_
