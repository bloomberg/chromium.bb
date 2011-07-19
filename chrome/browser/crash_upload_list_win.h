// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_WIN_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_WIN_H_
#pragma once

#include "chrome/browser/crash_upload_list.h"
#include "base/compiler_specific.h"

// A CrashUploadList that retrieves the list of reported crashes
// from the Windows Event Log.
class CrashUploadListWin : public CrashUploadList {
 public:
  explicit CrashUploadListWin(Delegate* delegate);

 protected:
  // Loads the list of crashes from the Windows Event Log.
  virtual void LoadCrashList() OVERRIDE;

 private:
  // Returns whether the event record is likely a Chrome crash log.
  bool IsPossibleCrashLogRecord(EVENTLOGRECORD* record) const;

  // Parses the event record and adds it to the crash list.
  void ProcessPossibleCrashLogRecord(EVENTLOGRECORD* record);

  DISALLOW_COPY_AND_ASSIGN(CrashUploadListWin);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_WIN_H_
