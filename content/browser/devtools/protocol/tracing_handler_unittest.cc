// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/trace_event/trace_config.h"
#include "base/values.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace protocol {

namespace {

const char kCustomTraceConfigString[] =
    "{"
    "\"enable_argument_filter\":true,"
    "\"enable_systrace\":true,"
    "\"excluded_categories\":[\"excluded\",\"exc_pattern*\"],"
    "\"included_categories\":[\"included\","
    "\"inc_pattern*\","
    "\"disabled-by-default-cc\","
    "\"disabled-by-default-memory-infra\"],"
    "\"memory_dump_config\":{"
    "\"allowed_dump_modes\":[\"background\",\"light\",\"detailed\"],"
    "\"triggers\":["
    "{"
    "\"min_time_between_dumps_ms\":50,"
    "\"mode\":\"light\","
    "\"type\":\"periodic_interval\""
    "},"
    "{"
    "\"min_time_between_dumps_ms\":1000,"
    "\"mode\":\"detailed\","
    "\"type\":\"periodic_interval\""
    "}"
    "]"
    "},"
    "\"record_mode\":\"record-continuously\""
    "}";

const char kCustomTraceConfigStringDevToolsStyle[] =
    "{"
    "\"enableArgumentFilter\":true,"
    "\"enableSystrace\":true,"
    "\"excludedCategories\":[\"excluded\",\"exc_pattern*\"],"
    "\"includedCategories\":[\"included\","
    "\"inc_pattern*\","
    "\"disabled-by-default-cc\","
    "\"disabled-by-default-memory-infra\"],"
    "\"memoryDumpConfig\":{"
    "\"allowedDumpModes\":[\"background\",\"light\",\"detailed\"],"
    "\"triggers\":["
    "{"
    "\"minTimeBetweenDumpsMs\":50,"
    "\"mode\":\"light\","
    "\"type\":\"periodic_interval\""
    "},"
    "{"
    "\"minTimeBetweenDumpsMs\":1000,"
    "\"mode\":\"detailed\","
    "\"type\":\"periodic_interval\""
    "}"
    "]"
    "},"
    "\"recordMode\":\"recordContinuously\""
    "}";

}  // namespace

TEST(TracingHandlerTest, GetTraceConfigFromDevToolsConfig) {
  std::unique_ptr<base::Value> value =
      base::JSONReader::Read(kCustomTraceConfigStringDevToolsStyle);
  std::unique_ptr<base::DictionaryValue> devtools_style_dict(
      static_cast<base::DictionaryValue*>(value.release()));

  base::trace_event::TraceConfig trace_config =
      TracingHandler::GetTraceConfigFromDevToolsConfig(*devtools_style_dict);

  EXPECT_STREQ(kCustomTraceConfigString, trace_config.ToString().c_str());
}

}  // namespace protocol
}  // namespace content
