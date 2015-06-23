// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/scoped_path_override.h"
#include "chromecast/crash/linux/minidump_generator.h"
#include "chromecast/crash/linux/minidump_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const char kDumplogFile[] = "dumplog";
const char kLockfileName[] = "lockfile";
const char kMinidumpSubdir[] = "minidumps";

std::string GetCurrentTimeASCII() {
  char cur_time[20];
  time_t now = time(NULL);
  struct tm* tm = gmtime(&now);
  strftime(cur_time, 20, "%Y-%m-%d %H:%M:%S", tm);
  return std::string(cur_time);
}

class FakeMinidumpGenerator : public MinidumpGenerator {
 public:
  FakeMinidumpGenerator() {}
  ~FakeMinidumpGenerator() override {}

  // MinidumpGenerator implementation:
  bool Generate(const std::string& minidump_path) override { return true; }
};

int FakeDumpState(const std::string& minidump_path) {
  return 0;
}

}  // namespace

class MinidumpWriterTest : public testing::Test {
 protected:
  MinidumpWriterTest() {}
  ~MinidumpWriterTest() override {}

  void SetUp() override {
    // Set up a temporary directory which will be used as our fake home dir.
    base::FilePath fake_home_dir;
    ASSERT_TRUE(base::CreateNewTempDirectory("", &fake_home_dir));
    home_.reset(new base::ScopedPathOverride(base::DIR_HOME, fake_home_dir));
    minidump_dir_ = fake_home_dir.Append(kMinidumpSubdir);
    dumplog_file_ = minidump_dir_.Append(kDumplogFile);

    // Create the minidump directory and lockfile.
    ASSERT_TRUE(base::CreateDirectory(minidump_dir_));
    base::File lockfile(
        minidump_dir_.Append(kLockfileName),
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(lockfile.IsValid());
  }

  FakeMinidumpGenerator fake_generator_;
  base::FilePath minidump_dir_;
  base::FilePath dumplog_file_;

 private:
  scoped_ptr<base::ScopedPathOverride> home_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpWriterTest);
};

TEST_F(MinidumpWriterTest, Write_FailsWithIncorrectMinidumpPath) {
  MinidumpWriter writer(&fake_generator_,
                        "/path/to/wrong/dir",
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  ASSERT_EQ(-1, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_FailsWithMultiLevelRelativeMinidumpPath) {
  MinidumpWriter writer(&fake_generator_,
                        "subdir/dumplog",
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  ASSERT_EQ(-1, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_SucceedsWithSimpleFilename) {
  MinidumpWriter writer(&fake_generator_,
                        "dumplog",
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  ASSERT_EQ(0, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_SucceedsWithCorrectMinidumpPath) {
  MinidumpWriter writer(&fake_generator_,
                        dumplog_file_.value(),
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  ASSERT_EQ(0, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_FailsWithSubdirInCorrectPath) {
  MinidumpWriter writer(&fake_generator_,
                        dumplog_file_.Append("subdir/logfile").value(),
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));
  ASSERT_EQ(-1, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_FailsWhenTooManyDumpsPresent) {
  MinidumpWriter writer(&fake_generator_,
                        dumplog_file_.value(),
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  // Write dump logs to the lockfile.
  std::ofstream lockfile(minidump_dir_.Append(kLockfileName).value());
  ASSERT_TRUE(lockfile.is_open());
  size_t too_many_dumps = writer.max_dumps() + 1;
  for (size_t i = 0; i < too_many_dumps; ++i) {
    lockfile << "p|2012-01-01 01:02:03|/dump/path||" << std::endl;
  }
  lockfile.close();

  ASSERT_EQ(-1, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_FailsWhenTooManyRecentDumpsPresent) {
  MinidumpWriter writer(&fake_generator_,
                        dumplog_file_.value(),
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  // Write dump logs to the lockfile.
  std::ofstream lockfile(minidump_dir_.Append(kLockfileName).value());
  ASSERT_TRUE(lockfile.is_open());
  size_t too_many_recent_dumps = writer.max_recent_dumps() + 1;
  for (size_t i = 0; i < too_many_recent_dumps; ++i) {
    lockfile << "|" << GetCurrentTimeASCII() << "|/dump/path||" << std::endl;
  }

  ASSERT_EQ(-1, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_SucceedsWhenDumpLimitsNotExceeded) {
  MinidumpWriter writer(&fake_generator_,
                        dumplog_file_.value(),
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  ASSERT_GT(writer.max_dumps(), 1);
  ASSERT_GT(writer.max_recent_dumps(), 0);

  // Write an old dump logs to the lockfile.
  std::ofstream lockfile(minidump_dir_.Append(kLockfileName).value());
  ASSERT_TRUE(lockfile.is_open());
  lockfile << "p|2012-01-01 01:02:03|/dump/path||" << std::endl;
}

}  // namespace chromecast
