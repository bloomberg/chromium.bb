// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
#pragma once

#include "base/memory/ref_counted.h"
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

  // Static factory method that creates the platform-specific implementation
  // of the crash upload list with the given callback delegate.
  static CrashUploadList* Create(Delegate* delegate);

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

 protected:
  virtual ~CrashUploadList();

  // Reads the upload log and stores the entries in crashes_.
  virtual void LoadCrashList();

  // Returns a reference to the list of crashes.
  std::vector<CrashInfo>& crashes();

 private:
  friend class base::RefCountedThreadSafe<CrashUploadList>;

  // Manages the background thread work for LoadCrashListAsynchronously().
  void LoadCrashListAndInformDelegateOfCompletion();

  // Calls the delegate's callback method, if there is a delegate.
  void InformDelegateOfCompletion();

  // Parses crash log lines, converting them to CrashInfo entries.
  void ParseLogEntries(const std::vector<std::string>& log_entries);

  std::vector<CrashInfo> crashes_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CrashUploadList);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_H_
