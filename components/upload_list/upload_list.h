// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_
#define COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base {
class SequencedTaskRunner;
class SequencedWorkerPool;
}

// Loads and parses an upload list text file of the format
// upload_time,upload_id[,local_id[,capture_time]]
// upload_time,upload_id[,local_id[,capture_time]]
// etc.
// where each line represents an upload. |upload_time| and |capture_time| are
// in Unix time. Must be used from the UI thread. The loading and parsing is
// done on a blocking pool task runner. A line may or may not contain
// |local_id| and |capture_time|.
class UploadList : public base::RefCountedThreadSafe<UploadList> {
 public:
  struct UploadInfo {
    UploadInfo(const std::string& upload_id,
               const base::Time& upload_time,
               const std::string& local_id,
               const base::Time& capture_time);
    UploadInfo(const std::string& upload_id, const base::Time& upload_time);
    ~UploadInfo();

    std::string upload_id;
    base::Time upload_time;

    // ID for locally stored data that may or may not be uploaded.
    std::string local_id;

    // The time the data was captured. This is useful if the data is stored
    // locally when captured and uploaded at a later time.
    base::Time capture_time;
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
             const base::FilePath& upload_log_path,
             const scoped_refptr<base::SequencedWorkerPool>& worker_pool);

  // Starts loading the upload list. OnUploadListAvailable will be called when
  // loading is complete.
  void LoadUploadListAsynchronously();

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

  // Clear |uploads_|.
  void ClearUploads();

 private:
  friend class base::RefCountedThreadSafe<UploadList>;
  FRIEND_TEST_ALL_PREFIXES(UploadListTest, ParseUploadTimeUploadId);
  FRIEND_TEST_ALL_PREFIXES(UploadListTest, ParseUploadTimeUploadIdLocalId);
  FRIEND_TEST_ALL_PREFIXES(UploadListTest, ParseUploadTimeUploadIdCaptureTime);
  FRIEND_TEST_ALL_PREFIXES(UploadListTest, ParseLocalIdCaptureTime);
  FRIEND_TEST_ALL_PREFIXES(UploadListTest,
                           ParseUploadTimeUploadIdLocalIdCaptureTime);
  FRIEND_TEST_ALL_PREFIXES(UploadListTest, ParseMultipleEntries);

  // Manages the background thread work for LoadUploadListAsynchronously().
  void LoadUploadListAndInformDelegateOfCompletion(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  // Calls the delegate's callback method, if there is a delegate.
  void InformDelegateOfCompletion();

  // Parses upload log lines, converting them to UploadInfo entries.
  void ParseLogEntries(const std::vector<std::string>& log_entries);

  std::vector<UploadInfo> uploads_;
  Delegate* delegate_;
  const base::FilePath upload_log_path_;

  base::ThreadChecker thread_checker_;
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;

  DISALLOW_COPY_AND_ASSIGN(UploadList);
};

#endif  // COMPONENTS_UPLOAD_LIST_UPLOAD_LIST_H_
