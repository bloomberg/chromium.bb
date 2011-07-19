// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTANT_FILE_WRITER_H_
#define CHROME_COMMON_IMPORTANT_FILE_WRITER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"
#include "base/timer.h"

namespace base {
class MessageLoopProxy;
class Thread;
}

// Helper to ensure that a file won't be corrupted by the write (for example on
// application crash). Consider a naive way to save an important file F:
//
// 1. Open F for writing, truncating it.
// 2. Write new data to F.
//
// It's good when it works, but it gets very bad if step 2. doesn't complete.
// It can be caused by a crash, a computer hang, or a weird I/O error. And you
// end up with a broken file.
//
// To be safe, we don't start with writing directly to F. Instead, we write to
// to a temporary file. Only after that write is successful, we rename the
// temporary file to target filename.
//
// If you want to know more about this approach and ext3/ext4 fsync issues, see
// http://valhenson.livejournal.com/37921.html
class ImportantFileWriter : public base::NonThreadSafe {
 public:
  // Used by ScheduleSave to lazily provide the data to be saved. Allows us
  // to also batch data serializations.
  class DataSerializer {
   public:
    virtual ~DataSerializer() {}

    // Should put serialized string in |data| and return true on successful
    // serialization. Will be called on the same thread on which
    // ImportantFileWriter has been created.
    virtual bool SerializeData(std::string* data) = 0;
  };

  // Initialize the writer.
  // |path| is the name of file to write.
  // |file_message_loop_proxy| is the MessageLoopProxy for a thread on which
  // file I/O can be done.
  // All non-const methods, ctor and dtor must be called on the same thread.
  ImportantFileWriter(const FilePath& path,
                      base::MessageLoopProxy* file_message_loop_proxy);

  // You have to ensure that there are no pending writes at the moment
  // of destruction.
  ~ImportantFileWriter();

  const FilePath& path() const { return path_; }

  // Returns true if there is a scheduled write pending which has not yet
  // been started.
  bool HasPendingWrite() const;

  // Save |data| to target filename. Does not block. If there is a pending write
  // scheduled by ScheduleWrite, it is cancelled.
  void WriteNow(const std::string& data);

  // Schedule a save to target filename. Data will be serialized and saved
  // to disk after the commit interval. If another ScheduleWrite is issued
  // before that, only one serialization and write to disk will happen, and
  // the most recent |serializer| will be used. This operation does not block.
  // |serializer| should remain valid through the lifetime of
  // ImportantFileWriter.
  void ScheduleWrite(DataSerializer* serializer);

  // Serialize data pending to be saved and execute write on backend thread.
  void DoScheduledWrite();

  base::TimeDelta commit_interval() const {
    return commit_interval_;
  }

  void set_commit_interval(const base::TimeDelta& interval) {
    commit_interval_ = interval;
  }

 private:
  // Path being written to.
  const FilePath path_;

  // MessageLoopProxy for the thread on which file I/O can be done.
  scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy_;

  // Timer used to schedule commit after ScheduleWrite.
  base::OneShotTimer<ImportantFileWriter> timer_;

  // Serializer which will provide the data to be saved.
  DataSerializer* serializer_;

  // Time delta after which scheduled data will be written to disk.
  base::TimeDelta commit_interval_;

  DISALLOW_COPY_AND_ASSIGN(ImportantFileWriter);
};

#endif  // CHROME_COMMON_IMPORTANT_FILE_WRITER_H_
