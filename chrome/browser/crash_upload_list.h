// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_H_

#include "chrome/browser/upload_list.h"

// An upload list manager for crash reports from breakpad.
class CrashUploadList : public UploadList {
 public:
  // Static factory method that creates the platform-specific implementation
  // of the crash upload list with the given callback delegate.
  static CrashUploadList* Create(Delegate* delegate);

  // Should match kReporterLogFilename in
  // breakpad/src/client/apple/Framework/BreakpadDefines.h.
  static const char* kReporterLogFilename;

  // Creates a new crash upload list with the given callback delegate.
  CrashUploadList(Delegate* delegate, const base::FilePath& upload_log_path);

 protected:
  virtual ~CrashUploadList();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashUploadList);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
