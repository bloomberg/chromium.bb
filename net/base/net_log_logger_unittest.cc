// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_logger.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "net/base/net_log_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class NetLogLoggerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    log_path_ = temp_dir_.path().AppendASCII("NetLogFile");
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath log_path_;
};

TEST_F(NetLogLoggerTest, GeneratesValidJSONForNoEvents) {
  // Create and destroy a logger.
  FILE* file = base::OpenFile(log_path_, "w");
  ASSERT_TRUE(file);
  scoped_ptr<base::Value> constants(GetNetConstants());
  scoped_ptr<NetLogLogger> logger(new NetLogLogger(file, *constants));
  logger.reset();

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  base::ListValue* events;
  ASSERT_TRUE(dict->GetList("events", &events));
  ASSERT_EQ(0u, events->GetSize());
}

// Make sure the log level is LOG_STRIP_PRIVATE_DATA by default.
TEST_F(NetLogLoggerTest, LogLevel) {
  FILE* file = base::OpenFile(log_path_, "w");
  ASSERT_TRUE(file);
  scoped_ptr<base::Value> constants(GetNetConstants());
  NetLogLogger logger(file, *constants);

  NetLog net_log;
  logger.StartObserving(&net_log);
  EXPECT_EQ(NetLog::LOG_STRIP_PRIVATE_DATA, logger.log_level());
  EXPECT_EQ(NetLog::LOG_STRIP_PRIVATE_DATA, net_log.GetLogLevel());
  logger.StopObserving();

  logger.set_log_level(NetLog::LOG_ALL_BUT_BYTES);
  logger.StartObserving(&net_log);
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, logger.log_level());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, net_log.GetLogLevel());
  logger.StopObserving();
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithOneEvent) {
  FILE* file = base::OpenFile(log_path_, "w");
  ASSERT_TRUE(file);
  scoped_ptr<base::Value> constants(GetNetConstants());
  scoped_ptr<NetLogLogger> logger(new NetLogLogger(file, *constants));

  const int kDummyId = 1;
  NetLog::Source source(NetLog::SOURCE_HTTP2_SESSION, kDummyId);
  NetLog::EntryData entry_data(NetLog::TYPE_PROXY_SERVICE,
                               source,
                               NetLog::PHASE_BEGIN,
                               base::TimeTicks::Now(),
                               NULL);
  NetLog::Entry entry(&entry_data, NetLog::LOG_ALL);
  logger->OnAddEntry(entry);
  logger.reset();

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  base::ListValue* events;
  ASSERT_TRUE(dict->GetList("events", &events));
  ASSERT_EQ(1u, events->GetSize());
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithMultipleEvents) {
  FILE* file = base::OpenFile(log_path_, "w");
  ASSERT_TRUE(file);
  scoped_ptr<base::Value> constants(GetNetConstants());
  scoped_ptr<NetLogLogger> logger(new NetLogLogger(file, *constants));

  const int kDummyId = 1;
  NetLog::Source source(NetLog::SOURCE_HTTP2_SESSION, kDummyId);
  NetLog::EntryData entry_data(NetLog::TYPE_PROXY_SERVICE,
                               source,
                               NetLog::PHASE_BEGIN,
                               base::TimeTicks::Now(),
                               NULL);
  NetLog::Entry entry(&entry_data, NetLog::LOG_ALL);

  // Add the entry multiple times.
  logger->OnAddEntry(entry);
  logger->OnAddEntry(entry);
  logger.reset();

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  base::ListValue* events;
  ASSERT_TRUE(dict->GetList("events", &events));
  ASSERT_EQ(2u, events->GetSize());
}

}  // namespace

}  // namespace net
