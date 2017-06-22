// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "content/browser/tracing/tracing_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TracingUITest : public testing::Test {
 public:
  TracingUITest() {}
};

std::string GetOldStyleConfig() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("categoryFilter", "filter1,-filter2");
  dict->SetString("tracingRecordMode", "record-continuously");
  dict->SetBoolean("useSystemTracing", true);

  std::string results;
  if (!base::JSONWriter::Write(*dict.get(), &results))
    return "";

  std::string data;
  base::Base64Encode(results, &data);
  return data;
}

std::string GetNewStyleConfig() {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  std::unique_ptr<base::Value> filter1(new base::Value("filter1"));
  std::unique_ptr<base::Value> filter2(new base::Value("filter2"));
  std::unique_ptr<base::ListValue> included(new base::ListValue);
  included->Append(std::move(filter1));
  std::unique_ptr<base::ListValue> excluded(new base::ListValue);
  excluded->Append(std::move(filter2));

  dict->SetList("included_categories", std::move(included));
  dict->SetList("excluded_categories", std::move(excluded));
  dict->SetString("record_mode", "record-continuously");
  dict->SetBoolean("enable_systrace", true);

  std::string results;
  if (!base::JSONWriter::Write(*dict.get(), &results))
    return "";

  std::string data;
  base::Base64Encode(results, &data);
  return data;
}

TEST_F(TracingUITest, OldStyleConfig) {
  base::trace_event::TraceConfig config;
  ASSERT_TRUE(TracingUI::GetTracingOptions(GetOldStyleConfig(), &config));
  EXPECT_EQ(config.GetTraceRecordMode(),
            base::trace_event::RECORD_CONTINUOUSLY);
  EXPECT_EQ(config.ToCategoryFilterString(), "filter1,-filter2");
  EXPECT_TRUE(config.IsSystraceEnabled());
}

TEST_F(TracingUITest, NewStyleConfig) {
  base::trace_event::TraceConfig config;
  ASSERT_TRUE(TracingUI::GetTracingOptions(GetNewStyleConfig(), &config));
  EXPECT_EQ(config.GetTraceRecordMode(),
            base::trace_event::RECORD_CONTINUOUSLY);
  EXPECT_EQ(config.ToCategoryFilterString(), "filter1,-filter2");
  EXPECT_TRUE(config.IsSystraceEnabled());
}

}  // namespace content
