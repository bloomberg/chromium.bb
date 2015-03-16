// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_logger.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/net_log.h"
#include "net/base/net_log_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
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
  NetLog net_log_;
};

TEST_F(NetLogLoggerTest, GeneratesValidJSONForNoEvents) {
  // Create and destroy a logger.
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  scoped_ptr<NetLogLogger> logger(new NetLogLogger());
  logger->StartObserving(&net_log_, file.Pass(), nullptr, nullptr);
  logger->StopObserving(nullptr);
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

  base::DictionaryValue* constants;
  ASSERT_TRUE(dict->GetDictionary("constants", &constants));
}

TEST_F(NetLogLoggerTest, LogLevel) {
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  NetLogLogger logger;
  logger.StartObserving(&net_log_, file.Pass(), nullptr, nullptr);
  EXPECT_EQ(NetLog::LOG_STRIP_PRIVATE_DATA, logger.log_level());
  EXPECT_EQ(NetLog::LOG_STRIP_PRIVATE_DATA, net_log_.GetLogLevel());
  logger.StopObserving(nullptr);

  file.reset(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  logger.set_log_level(NetLog::LOG_ALL_BUT_BYTES);
  logger.StartObserving(&net_log_, file.Pass(), nullptr, nullptr);
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, logger.log_level());
  EXPECT_EQ(NetLog::LOG_ALL_BUT_BYTES, net_log_.GetLogLevel());
  logger.StopObserving(nullptr);
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithOneEvent) {
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  scoped_ptr<NetLogLogger> logger(new NetLogLogger());
  logger->StartObserving(&net_log_, file.Pass(), nullptr, nullptr);

  const int kDummyId = 1;
  NetLog::Source source(NetLog::SOURCE_HTTP2_SESSION, kDummyId);
  NetLog::EntryData entry_data(NetLog::TYPE_PROXY_SERVICE,
                               source,
                               NetLog::PHASE_BEGIN,
                               base::TimeTicks::Now(),
                               NULL);
  NetLog::Entry entry(&entry_data, NetLog::LOG_ALL);
  logger->OnAddEntry(entry);
  logger->StopObserving(nullptr);
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
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  scoped_ptr<NetLogLogger> logger(new NetLogLogger());
  logger->StartObserving(&net_log_, file.Pass(), nullptr, nullptr);

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
  logger->StopObserving(nullptr);
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

TEST_F(NetLogLoggerTest, CustomConstants) {
  const char kConstantString[] = "awesome constant";
  scoped_ptr<base::Value> constants(new base::StringValue(kConstantString));
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  scoped_ptr<NetLogLogger> logger(new NetLogLogger());
  logger->StartObserving(&net_log_, file.Pass(), constants.get(), nullptr);
  logger->StopObserving(nullptr);
  logger.reset();

  std::string input;
  ASSERT_TRUE(base::ReadFileToString(log_path_, &input));

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root) << reader.GetErrorMessage();

  base::DictionaryValue* dict;
  ASSERT_TRUE(root->GetAsDictionary(&dict));
  std::string constants_string;
  ASSERT_TRUE(dict->GetString("constants", &constants_string));
  ASSERT_EQ(kConstantString, constants_string);
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithContext) {
  // Create context, start a request.
  TestURLRequestContext context(true);
  context.set_net_log(&net_log_);
  context.Init();

  // Create and destroy a logger.
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  scoped_ptr<NetLogLogger> logger(new NetLogLogger());
  logger->StartObserving(&net_log_, file.Pass(), nullptr, &context);
  logger->StopObserving(&context);
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

  // Make sure additional information is present, but don't validate it.
  base::DictionaryValue* tab_info;
  ASSERT_TRUE(dict->GetDictionary("tabInfo", &tab_info));
}

TEST_F(NetLogLoggerTest, GeneratesValidJSONWithContextWithActiveRequest) {
  // Create context, start a request.
  TestURLRequestContext context(true);
  context.set_net_log(&net_log_);
  context.Init();
  TestDelegate delegate;

  // URL doesn't matter.  Requests can't fail synchronously.
  scoped_ptr<URLRequest> request(
      context.CreateRequest(GURL("blah:blah"), IDLE, &delegate, nullptr));
  request->Start();

  // Create and destroy a logger.
  base::ScopedFILE file(base::OpenFile(log_path_, "w"));
  ASSERT_TRUE(file);
  scoped_ptr<NetLogLogger> logger(new NetLogLogger());
  logger->StartObserving(&net_log_, file.Pass(), nullptr, &context);
  logger->StopObserving(&context);
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

  // Make sure additional information is present, but don't validate it.
  base::DictionaryValue* tab_info;
  ASSERT_TRUE(dict->GetDictionary("tabInfo", &tab_info));
}

}  // namespace

}  // namespace net
