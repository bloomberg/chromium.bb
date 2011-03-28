// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/important_file_writer.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetFileContent(const FilePath& path) {
  std::string content;
  if (!file_util::ReadFileToString(path, &content)) {
    NOTREACHED();
  }
  return content;
}

class DataSerializer : public ImportantFileWriter::DataSerializer {
 public:
  explicit DataSerializer(const std::string& data) : data_(data) {
  }

  virtual bool SerializeData(std::string* output) {
    output->assign(data_);
    return true;
  }

 private:
  const std::string data_;
};

}  // namespace

class ImportantFileWriterTest : public testing::Test {
 public:
  ImportantFileWriterTest() { }
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.path().AppendASCII("test-file");
  }

 protected:
  FilePath file_;
  MessageLoop loop_;

 private:
  ScopedTempDir temp_dir_;
};

TEST_F(ImportantFileWriterTest, Basic) {
  ImportantFileWriter writer(file_,
                             base::MessageLoopProxy::CreateForCurrentThread());
  EXPECT_FALSE(file_util::PathExists(writer.path()));
  writer.WriteNow("foo");
  loop_.RunAllPending();

  ASSERT_TRUE(file_util::PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, ScheduleWrite) {
  ImportantFileWriter writer(file_,
                             base::MessageLoopProxy::CreateForCurrentThread());
  writer.set_commit_interval(base::TimeDelta::FromMilliseconds(25));
  EXPECT_FALSE(writer.HasPendingWrite());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 100);
  MessageLoop::current()->Run();
  EXPECT_FALSE(writer.HasPendingWrite());
  ASSERT_TRUE(file_util::PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, DoScheduledWrite) {
  ImportantFileWriter writer(file_,
                             base::MessageLoopProxy::CreateForCurrentThread());
  EXPECT_FALSE(writer.HasPendingWrite());
  DataSerializer serializer("foo");
  writer.ScheduleWrite(&serializer);
  EXPECT_TRUE(writer.HasPendingWrite());
  writer.DoScheduledWrite();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 100);
  MessageLoop::current()->Run();
  EXPECT_FALSE(writer.HasPendingWrite());
  ASSERT_TRUE(file_util::PathExists(writer.path()));
  EXPECT_EQ("foo", GetFileContent(writer.path()));
}

TEST_F(ImportantFileWriterTest, BatchingWrites) {
  ImportantFileWriter writer(file_,
                             base::MessageLoopProxy::CreateForCurrentThread());
  writer.set_commit_interval(base::TimeDelta::FromMilliseconds(25));
  DataSerializer foo("foo"), bar("bar"), baz("baz");
  writer.ScheduleWrite(&foo);
  writer.ScheduleWrite(&bar);
  writer.ScheduleWrite(&baz);
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 100);
  MessageLoop::current()->Run();
  ASSERT_TRUE(file_util::PathExists(writer.path()));
  EXPECT_EQ("baz", GetFileContent(writer.path()));
}
