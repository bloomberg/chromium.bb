// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_test_util.h"

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_api_parser.h"
#include "chrome/browser/chromeos/gdata/drive_file_system.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

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

DriveCacheEntry ToCacheEntry(int cache_state) {
  DriveCacheEntry cache_entry;
  cache_entry.set_is_present(cache_state & TEST_CACHE_STATE_PRESENT);
  cache_entry.set_is_pinned(cache_state & TEST_CACHE_STATE_PINNED);
  cache_entry.set_is_dirty(cache_state & TEST_CACHE_STATE_DIRTY);
  cache_entry.set_is_mounted(cache_state & TEST_CACHE_STATE_MOUNTED);
  cache_entry.set_is_persistent(cache_state & TEST_CACHE_STATE_PERSISTENT);
  return cache_entry;
}

bool CacheStatesEqual(const DriveCacheEntry& a, const DriveCacheEntry& b) {
  return (a.is_present() == b.is_present() &&
          a.is_pinned() == b.is_pinned() &&
          a.is_dirty() == b.is_dirty() &&
          a.is_mounted() == b.is_mounted() &&
          a.is_persistent() == b.is_persistent());
}

void CopyErrorCodeFromFileOperationCallback(DriveFileError* output,
                                            DriveFileError error) {
  DCHECK(output);
  *output = error;
}

void CopyResultsFromFileMoveCallback(
    DriveFileError* out_error,
    FilePath* out_file_path,
    DriveFileError error,
    const FilePath& moved_file_path) {
  DCHECK(out_error);
  DCHECK(out_file_path);

  *out_error = error;
  *out_file_path = moved_file_path;
}

void CopyResultsFromGetEntryInfoCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(out_error);
  DCHECK(out_entry_proto);

  *out_error = error;
  *out_entry_proto = entry_proto.Pass();
}

void CopyResultsFromReadDirectoryCallback(
    DriveFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    DriveFileError error,
    scoped_ptr<DriveEntryProtoVector> entries) {
  DCHECK(out_error);
  DCHECK(out_entries);

  *out_error = error;
  *out_entries = entries.Pass();
}

void CopyResultsFromGetEntryInfoWithFilePathCallback(
    DriveFileError* out_error,
    FilePath* out_drive_file_path,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    DriveFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto) {
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

FilePath GetTestFilePath(const std::string& relative_path) {
  FilePath path;
  std::string error;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("chromeos")
      .Append(FilePath::FromUTF8Unsafe(relative_path));
  EXPECT_TRUE(file_util::PathExists(path)) <<
      "Couldn't find " << path.value();
  return path;
}

base::Value* LoadJSONFile(const std::string& relative_path) {
  FilePath path = GetTestFilePath(relative_path);

  std::string error;
  JSONFileValueSerializer serializer(path);
  base::Value* value = serializer.Deserialize(NULL, &error);
  EXPECT_TRUE(value) <<
      "Parse error " << path.value() << ": " << error;
  return value;
}

void LoadChangeFeed(const std::string& relative_path,
                    DriveFileSystem* file_system,
                    int64 start_changestamp,
                    int64 root_feed_changestamp) {
  std::string error;
  scoped_ptr<Value> document(test_util::LoadJSONFile(relative_path));
  ASSERT_TRUE(document.get());
  ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
  scoped_ptr<DocumentFeed> document_feed(
      DocumentFeed::ExtractAndParse(*document));
  ASSERT_TRUE(document_feed.get());
  ScopedVector<DocumentFeed> feed_list;
  feed_list.push_back(document_feed.release());

  GURL unused;
  DriveFileError file_error = file_system->UpdateFromFeedForTesting(
      feed_list,
      start_changestamp,
      root_feed_changestamp);
  ASSERT_EQ(DRIVE_FILE_OK, file_error);
}

}  // namespace test_util
}  // namespace gdata
