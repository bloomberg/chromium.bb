// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
#pragma once

#include "base/ref_counted.h"
#include "base/time.h"

#include <string>
#include <vector>

class CrashUploadList : public base::RefCountedThreadSafe<CrashUploadList> {
 public:
  struct CrashInfo {
    CrashInfo(const std::string& c, const base::Time& t);
    ~CrashInfo();

    std::string crash_id;
    base::Time crash_time;
  };

  class Delegate {
   public:
    // Invoked when the crash list has been loaded. Will be called on the
    // UI thread.
    virtual void OnCrashListAvailable() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a new crash upload list with the given callback delegate.
  explicit CrashUploadList(Delegate* delegate);

  // Starts loading the crash list. OnCrashListAvailable will be called when
  // loading is complete.
  void LoadCrashListAsynchronously();

  // Clears the delegate, so that any outstanding asynchronous load will not
  // call the delegate on completion.
  void ClearDelegate();

  // Populates |crashes| with the |max_count| most recent uploaded crashes,
  // in reverse chronological order.
  // Must be called only after OnCrashListAvailable has been called.
  void GetUploadedCrashes(unsigned int max_count,
                          std::vector<CrashInfo>* crashes);

 private:
  friend class base::RefCountedThreadSafe<CrashUploadList>;
  virtual ~CrashUploadList();

  // Reads the upload log and stores the lines in log_entries_.
  void LoadUploadLog();

  // Calls the delegate's callback method, if there is a delegate.
  void InformDelegateOfCompletion();

  std::vector<std::string> log_entries_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CrashUploadList);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
