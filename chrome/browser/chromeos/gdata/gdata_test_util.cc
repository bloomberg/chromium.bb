// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_test_util.h"

#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "content/public/browser/browser_thread.h"

namespace gdata {
namespace test_util {

// This class is used to monitor if any task is posted to a message loop.
class TaskObserver : public MessageLoop::TaskObserver {
 public:
  TaskObserver() : posted_(false) {}
  virtual ~TaskObserver() {}

  // MessageLoop::TaskObserver overrides.
  virtual void WillProcessTask(base::TimeTicks time_posted) {}
  virtual void DidProcessTask(base::TimeTicks time_posted) {
    posted_ = true;
  }

  // Returns true if any task was posted.
  bool posted() const { return posted_; }

 private:
  bool posted_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

void RunBlockingPoolTask() {
  while (true) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    TaskObserver task_observer;
    MessageLoop::current()->AddTaskObserver(&task_observer);
    MessageLoop::current()->RunAllPending();
    MessageLoop::current()->RemoveTaskObserver(&task_observer);
    if (!task_observer.posted())
      break;
  }
}

GDataCacheEntry ToCacheEntry(int cache_state) {
  GDataCacheEntry cache_entry;
  cache_entry.set_is_present(cache_state & TEST_CACHE_STATE_PRESENT);
  cache_entry.set_is_pinned(cache_state & TEST_CACHE_STATE_PINNED);
  cache_entry.set_is_dirty(cache_state & TEST_CACHE_STATE_DIRTY);
  cache_entry.set_is_mounted(cache_state & TEST_CACHE_STATE_MOUNTED);
  cache_entry.set_is_persistent(cache_state & TEST_CACHE_STATE_PERSISTENT);
  return cache_entry;
}

bool CacheStatesEqual(const GDataCacheEntry& a, const GDataCacheEntry& b) {
  return (a.is_present() == b.is_present() &&
          a.is_pinned() == b.is_pinned() &&
          a.is_dirty() == b.is_dirty() &&
          a.is_mounted() == b.is_mounted() &&
          a.is_persistent() == b.is_persistent());
}

void CopyErrorCodeFromFileOperationCallback(GDataFileError* output,
                                            GDataFileError error) {
  DCHECK(output);
  *output = error;
}

void CopyResultsFromFileMoveCallback(
    GDataFileError* out_error,
    FilePath* out_file_path,
    GDataFileError error,
    const FilePath& moved_file_path) {
  DCHECK(out_error);
  DCHECK(out_file_path);

  *out_error = error;
  *out_file_path = moved_file_path;
}

void CopyResultsFromGetEntryInfoCallback(
    GDataFileError* out_error,
    scoped_ptr<GDataEntryProto>* out_entry_proto,
    GDataFileError error,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(out_error);
  DCHECK(out_entry_proto);

  *out_error = error;
  *out_entry_proto = entry_proto.Pass();
}

void CopyResultsFromReadDirectoryCallback(
    GDataFileError* out_error,
    scoped_ptr<GDataEntryProtoVector>* out_entries,
    GDataFileError error,
    scoped_ptr<GDataEntryProtoVector> entries) {
  DCHECK(out_error);
  DCHECK(out_entries);

  *out_error = error;
  *out_entries = entries.Pass();
}

void CopyResultsFromGetEntryInfoWithFilePathCallback(
    GDataFileError* out_error,
    FilePath* out_drive_file_path,
    scoped_ptr<GDataEntryProto>* out_entry_proto,
    GDataFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<GDataEntryProto> entry_proto) {
  DCHECK(out_error);
  DCHECK(out_drive_file_path);
  DCHECK(out_entry_proto);

  *out_error = error;
  *out_drive_file_path = drive_file_path;
  *out_entry_proto = entry_proto.Pass();
}

void CopyResultsFromGetEntryInfoPairCallback(
    scoped_ptr<EntryInfoPairResult>* out_result,
    scoped_ptr<EntryInfoPairResult> result) {
  DCHECK(out_result);

  *out_result = result.Pass();
}

}  // namespace test_util
}  // namespace gdata
