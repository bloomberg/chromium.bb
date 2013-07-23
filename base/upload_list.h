// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UPLOAD_LIST_H_
#define BASE_UPLOAD_LIST_H_

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {

class SingleThreadTaskRunner;
class SequencedWorkerPool;

// Loads and parses an upload list text file of the format
// time,id
// time,id
// etc.
// where each line represents an upload and "time" is Unix time. Must be used
// from the UI thread. The loading and parsing is done on a blocking pool task
// runner.
class BASE_EXPORT UploadList : public RefCountedThreadSafe<UploadList> {
 public:
  struct BASE_EXPORT UploadInfo {
    UploadInfo(const std::string& c, const Time& t);
    ~UploadInfo();

    std::string id;
    Time time;
  };

  class Delegate {
   public:
    // Invoked when the upload list has been loaded. Will be called on the
    // UI thread.
    virtual void OnUploadListAvailable() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a new upload list with the given callback delegate.
  UploadList(Delegate* delegate,
             const FilePath& upload_log_path,
             SingleThreadTaskRunner* task_runner);

  // Starts loading the upload list. OnUploadListAvailable will be called when
  // loading is complete.
  void LoadUploadListAsynchronously(SequencedWorkerPool* worker_pool);

  // Clears the delegate, so that any outstanding asynchronous load will not
  // call the delegate on completion.
  void ClearDelegate();

  // Populates |uploads| with the |max_count| most recent uploads,
  // in reverse chronological order.
  // Must be called only after OnUploadListAvailable has been called.
  void GetUploads(unsigned int max_count, std::vector<UploadInfo>* uploads);

 protected:
  virtual ~UploadList();

  // Reads the upload log and stores the entries in |uploads_|.
  virtual void LoadUploadList();

  // Adds |info| to |uploads_|.
  void AppendUploadInfo(const UploadInfo& info);

 private:
  friend class RefCountedThreadSafe<UploadList>;
  FRIEND_TEST_ALL_PREFIXES(UploadListTest, ParseLogEntries);

  // Manages the background thread work for LoadUploadListAsynchronously().
  void LoadUploadListAndInformDelegateOfCompletion();

  // Calls the delegate's callback method, if there is a delegate.
  void InformDelegateOfCompletion();

  // Parses upload log lines, converting them to UploadInfo entries.
  void ParseLogEntries(const std::vector<std::string>& log_entries);

  std::vector<UploadInfo> uploads_;
  Delegate* delegate_;
  const FilePath upload_log_path_;
  SingleThreadTaskRunner* task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UploadList);
};

}  // namespace base

#endif  // BASE_UPLOAD_LIST_H_
