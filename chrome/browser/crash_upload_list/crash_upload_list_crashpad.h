// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CRASHPAD_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CRASHPAD_H_

#include "base/macros.h"
#include "components/upload_list/crash_upload_list.h"

namespace base {
class TaskRunner;
}

// A CrashUploadList that retrieves the list of uploaded reports from the
// Crashpad database.
class CrashUploadListCrashpad : public CrashUploadList {
 public:
  CrashUploadListCrashpad(Delegate* delegate,
                          scoped_refptr<base::TaskRunner> task_runner);

 protected:
  ~CrashUploadListCrashpad() override;

  // Called on a blocking pool thread.
  void LoadUploadList(std::vector<UploadInfo>* uploads) override;

  void RequestSingleCrashUpload(const std::string& local_id) override;

  DISALLOW_COPY_AND_ASSIGN(CrashUploadListCrashpad);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_CRASHPAD_H_
