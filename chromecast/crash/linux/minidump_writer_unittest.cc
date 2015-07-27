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
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"
#include "chromecast/crash/linux/minidump_generator.h"
#include "chromecast/crash/linux/minidump_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace {

const char kDumplogFile[] = "dumplog";
const char kLockfileName[] = "lockfile";
const char kMinidumpSubdir[] = "minidumps";
const char kDumpsKey[] = "dumps";

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

scoped_ptr<DumpInfo> CreateDumpInfo(const std::string& json_string) {
  scoped_ptr<base::Value> value(DeserializeFromJson(json_string));
  return make_scoped_ptr(new DumpInfo(*value));
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
    lockfile_path_ = minidump_dir_.Append(kLockfileName);

    // Create the minidump directory and lockfile.
    ASSERT_TRUE(base::CreateDirectory(minidump_dir_));
    base::File lockfile(
        lockfile_path_,
        base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    ASSERT_TRUE(lockfile.IsValid());
  }

  int AppendLockFile(const DumpInfo& dump) {
    scoped_ptr<base::Value> contents(DeserializeJsonFromFile(lockfile_path_));
    if (!contents) {
      base::DictionaryValue* dict = new base::DictionaryValue();
      contents = make_scoped_ptr(dict);
      dict->Set(kDumpsKey, make_scoped_ptr(new base::ListValue()));
    }

    base::DictionaryValue* dict;
    base::ListValue* dump_list;
    if (!contents || !contents->GetAsDictionary(&dict) ||
        !dict->GetList(kDumpsKey, &dump_list) || !dump_list) {
      return -1;
    }

    dump_list->Append(dump.GetAsValue());

    return SerializeJsonToFile(lockfile_path_, *contents) ? 0 : -1;
  }

  FakeMinidumpGenerator fake_generator_;
  base::FilePath minidump_dir_;
  base::FilePath dumplog_file_;
  base::FilePath lockfile_path_;

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
  size_t too_many_dumps = writer.max_dumps() + 1;
  for (size_t i = 0; i < too_many_dumps; ++i) {
    scoped_ptr<DumpInfo> info(CreateDumpInfo(
        "{"
        "\"name\": \"p\","
        "\"dump_time\" : \"2012-01-01 01:02:03\","
        "\"dump\": \"dump_string\","
        "\"uptime\": \"123456789\","
        "\"logfile\": \"logfile.log\""
        "}"));
    ASSERT_TRUE(info->valid());
    ASSERT_EQ(0, AppendLockFile(*info));
  }

  ASSERT_EQ(-1, writer.Write());
}

TEST_F(MinidumpWriterTest, Write_FailsWhenTooManyRecentDumpsPresent) {
  MinidumpWriter writer(&fake_generator_,
                        dumplog_file_.value(),
                        MinidumpParams(),
                        base::Bind(&FakeDumpState));

  // Write dump logs to the lockfile.
  size_t too_many_recent_dumps = writer.max_recent_dumps() + 1;
  for (size_t i = 0; i < too_many_recent_dumps; ++i) {
    MinidumpParams params;
    DumpInfo info("dump", "/dump/path", time(nullptr), params);
    ASSERT_EQ(0, AppendLockFile(info));
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
  scoped_ptr<DumpInfo> info(CreateDumpInfo(
      "{"
      "\"name\": \"p\","
      "\"dump_time\" : \"2012-01-01 01:02:03\","
      "\"dump\": \"dump_string\","
      "\"uptime\": \"123456789\","
      "\"logfile\": \"logfile.log\""
      "}"));
  ASSERT_TRUE(info->valid());
  ASSERT_EQ(0, AppendLockFile(*info));
  ASSERT_EQ(0, writer.Write());
}

}  // namespace chromecast
