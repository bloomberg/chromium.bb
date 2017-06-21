// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"

#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/feedback_private/log_source_resource.h"
#include "chrome/browser/extensions/api/feedback_private/single_log_source_factory.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

namespace {

using api::feedback_private::ReadLogSourceResult;
using api::feedback_private::ReadLogSourceParams;
using base::TimeDelta;
using system_logs::SingleLogSource;
using system_logs::SystemLogsResponse;
using SupportedSource = system_logs::SingleLogSource::SupportedSource;

std::unique_ptr<KeyedService> ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  return base::MakeUnique<ApiResourceManager<LogSourceResource>>(context);
}

// Converts |params| to a string containing a JSON dictionary within an argument
// list.
std::string ParamsToJSON(const ReadLogSourceParams& params) {
  base::ListValue params_value;
  params_value.Append(params.ToValue());
  std::string params_json_string;
  EXPECT_TRUE(base::JSONWriter::Write(params_value, &params_json_string));

  return params_json_string;
}

// A dummy SingleLogSource that does not require real system logs to be
// available during testing.
class TestSingleLogSource : public SingleLogSource {
 public:
  explicit TestSingleLogSource(SupportedSource type)
      : SingleLogSource(type), call_count_(0) {}

  ~TestSingleLogSource() override = default;

  // Fetch() will return a single different string each time, in the following
  // sequence: "a", " bb", "  ccc", until 25 spaces followed by 26 z's. Will
  // never return an empty result.
  void Fetch(const system_logs::SysLogsSourceCallback& callback) override {
    int count_modulus = call_count_ % kNumCharsToIterate;
    std::string result =
        std::string(count_modulus, ' ') +
        std::string(count_modulus + 1, kInitialChar + count_modulus);
    ASSERT_GT(result.size(), 0U);
    ++call_count_;

    SystemLogsResponse* result_map = new SystemLogsResponse;
    result_map->emplace("", result);

    // Do not directly pass the result to the callback, because that's not how
    // log sources actually work. Instead, simulate the asynchronous operation
    // of a SingleLogSource by invoking the callback separately.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, base::Owned(result_map)));
  }

  // Instantiates a new instance of this class. Does not retain ownership. Used
  // to create a Callback that can be used to override the default behavior of
  // SingleLogSourceFactory.
  static std::unique_ptr<SingleLogSource> Create(SupportedSource type) {
    return base::MakeUnique<TestSingleLogSource>(type);
  }

 private:
  // Iterate over the whole lowercase alphabet, starting from 'a'.
  const int kNumCharsToIterate = 26;
  const char kInitialChar = 'a';

  // Keep track of how many times Fetch() has been called, in order to determine
  // its behavior each time.
  int call_count_;

  DISALLOW_COPY_AND_ASSIGN(TestSingleLogSource);
};

}  // namespace

class FeedbackPrivateApiUnittest : public ExtensionApiUnittest {
 public:
  FeedbackPrivateApiUnittest()
      : create_callback_(base::Bind(&TestSingleLogSource::Create)) {}
  ~FeedbackPrivateApiUnittest() override {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();

    // The ApiResourceManager used for LogSourceResource is destroyed every time
    // a unit test finishes, during TearDown(). There is no way to re-create it
    // normally. The below code forces it to be re-created during SetUp(), so
    // that there is always a valid ApiResourceManager<LogSourceResource> when
    // subsequent unit tests are running.
    ApiResourceManager<LogSourceResource>::GetFactoryInstance()
        ->SetTestingFactoryAndUse(profile(), ApiResourceManagerTestFactory);

    SingleLogSourceFactory::SetForTesting(&create_callback_);
  }

  void TearDown() override {
    SingleLogSourceFactory::SetForTesting(nullptr);
    LogSourceAccessManager::SetRateLimitingTimeoutForTesting(nullptr);

    FeedbackPrivateAPI::GetFactoryInstance()
        ->Get(profile())
        ->GetLogSourceAccessManager()
        ->SetTickClockForTesting(nullptr);

    ExtensionApiUnittest::TearDown();
  }

  // Runs the feedbackPrivate.readLogSource() function. See API function
  // definition for argument descriptions.
  //
  // The API function is expected to complete successfully. For running the
  // function with an expectation of an error result, call
  // RunReadLogSourceFunctionWithError().
  //
  // Note that the second argument of the result is a list of strings, but the
  // test class TestSingleLogSource always returns a list containing a single
  // string. To simplify things, the single string result will be returned in
  // |*result_string|, while the reader ID is returned in |*result_reader_id|.
  testing::AssertionResult RunReadLogSourceFunction(
      const ReadLogSourceParams& params,
      int* result_reader_id,
      std::string* result_string) {
    scoped_refptr<FeedbackPrivateReadLogSourceFunction> function =
        new FeedbackPrivateReadLogSourceFunction;

    std::unique_ptr<base::Value> result_value =
        RunFunctionAndReturnValue(function.get(), ParamsToJSON(params));
    if (!result_value)
      return testing::AssertionFailure() << "No result";

    ReadLogSourceResult result;
    if (!ReadLogSourceResult::Populate(*result_value, &result)) {
      return testing::AssertionFailure()
             << "Unable to parse a valid result from " << *result_value;
    }

    if (result.log_lines.size() != 1) {
      return testing::AssertionFailure()
             << "Expected |log_lines| to contain 1 string, actual number: "
             << result.log_lines.size();
    }

    *result_reader_id = result.reader_id;
    *result_string = result.log_lines[0];

    return testing::AssertionSuccess();
  }

  // Similar to RunReadLogSourceFunction(), but expects to return an error.
  // Returns a string containing the error message. Does not return any result
  // from the API function.
  std::string RunReadLogSourceFunctionWithError(
      const ReadLogSourceParams& params) {
    scoped_refptr<FeedbackPrivateReadLogSourceFunction> function =
        new FeedbackPrivateReadLogSourceFunction;

    return RunFunctionAndReturnError(function.get(), ParamsToJSON(params));
  }

 private:
  // Passed to SingleLogSourceFactory so that the API can create an instance of
  // TestSingleLogSource for testing.
  SingleLogSourceFactory::CreateCallback create_callback_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackPrivateApiUnittest);
};

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceInvalidId) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;
  params.reader_id.reset(new int(9999));

  EXPECT_NE("", RunReadLogSourceFunctionWithError(params));
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceNonIncremental) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = false;

  // Test multiple non-incremental reads.
  int result_reader_id = -1;
  std::string result_string;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("a", result_string);

  result_reader_id = -1;
  result_string.clear();
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("a", result_string);

  result_reader_id = -1;
  result_string.clear();
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("a", result_string);
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceIncremental) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;

  int result_reader_id = 0;
  std::string result_string;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_EQ("a", result_string);
  params.reader_id.reset(new int(result_reader_id));

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(*params.reader_id, result_reader_id);
  EXPECT_EQ(" bb", result_string);

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(*params.reader_id, result_reader_id);
  EXPECT_EQ("  ccc", result_string);

  // End the incremental read.
  params.incremental = false;
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(0, result_reader_id);
  EXPECT_EQ("   dddd", result_string);

  // The log source will no longer be valid if we try to read it.
  params.incremental = true;
  EXPECT_NE("", RunReadLogSourceFunctionWithError(params));
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceMultipleSources) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  int result_reader_id = 0;
  std::string result_string;

  // Attempt to open LOG_SOURCE_MESSAGES twice.
  ReadLogSourceParams params_1st_read;
  params_1st_read.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params_1st_read.incremental = true;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read, &result_reader_id,
                                       &result_string));
  EXPECT_GT(result_reader_id, 0);
  // Store the reader ID back into the params to set up for the next call.
  params_1st_read.reader_id = base::MakeUnique<int>(result_reader_id);

  // Cannot create a second reader from the same log source.
  ReadLogSourceParams params_1st_read_repeated;
  params_1st_read_repeated.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params_1st_read_repeated.incremental = true;
  EXPECT_NE("", RunReadLogSourceFunctionWithError(params_1st_read_repeated));

  // Attempt to open LOG_SOURCE_UI_LATEST twice.
  ReadLogSourceParams params_2nd_read;
  params_2nd_read.source = api::feedback_private::LOG_SOURCE_UILATEST;
  params_2nd_read.incremental = true;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read, &result_reader_id,
                                       &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_NE(*params_1st_read.reader_id, result_reader_id);
  // Store the reader ID back into the params to set up for the next call.
  params_2nd_read.reader_id = base::MakeUnique<int>(result_reader_id);

  // Cannot create a second reader from the same log source.
  ReadLogSourceParams params_2nd_read_repeated;
  params_2nd_read_repeated.source = api::feedback_private::LOG_SOURCE_UILATEST;
  params_2nd_read_repeated.incremental = true;
  EXPECT_NE("", RunReadLogSourceFunctionWithError(params_2nd_read_repeated));

  // Close the two open log source readers, and make sure new ones can be
  // opened.
  params_1st_read.incremental = false;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read, &result_reader_id,
                                       &result_string));
  EXPECT_EQ(0, result_reader_id);

  params_2nd_read.incremental = false;
  result_reader_id = -1;
  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read, &result_reader_id,
                                       &result_string));
  EXPECT_EQ(0, result_reader_id);

  EXPECT_TRUE(RunReadLogSourceFunction(params_1st_read_repeated,
                                       &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  const int new_read_result_reader_id = result_reader_id;

  EXPECT_TRUE(RunReadLogSourceFunction(params_2nd_read_repeated,
                                       &result_reader_id, &result_string));
  EXPECT_GT(result_reader_id, 0);
  EXPECT_NE(new_read_result_reader_id, result_reader_id);
}

TEST_F(FeedbackPrivateApiUnittest, ReadLogSourceWithAccessTimeouts) {
  const TimeDelta timeout(TimeDelta::FromMilliseconds(100));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  base::SimpleTestTickClock* test_clock = new base::SimpleTestTickClock;
  FeedbackPrivateAPI::GetFactoryInstance()
      ->Get(profile())
      ->GetLogSourceAccessManager()
      ->SetTickClockForTesting(std::unique_ptr<base::TickClock>(test_clock));

  ReadLogSourceParams params;
  params.source = api::feedback_private::LOG_SOURCE_MESSAGES;
  params.incremental = true;
  int result_reader_id = 0;
  std::string result_string;

  // |test_clock| must start out at something other than 0, which is interpreted
  // as an invalid value.
  test_clock->Advance(TimeDelta::FromMilliseconds(100));

  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
  EXPECT_EQ(1, result_reader_id);
  params.reader_id.reset(new int(result_reader_id));

  // Immediately perform another read. This is not allowed. (empty result)
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=120, but it will not be allowed. (empty result)
  test_clock->Advance(TimeDelta::FromMilliseconds(20));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=150, but still not allowed.
  test_clock->Advance(TimeDelta::FromMilliseconds(30));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=199, but still not allowed. (empty result)
  test_clock->Advance(TimeDelta::FromMilliseconds(49));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=210, annd the access is finally allowed.
  test_clock->Advance(TimeDelta::FromMilliseconds(11));
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Advance to t=309, but it will not be allowed. (empty result)
  test_clock->Advance(TimeDelta::FromMilliseconds(99));
  EXPECT_FALSE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));

  // Another read is finally allowed at t=310.
  test_clock->Advance(TimeDelta::FromMilliseconds(1));
  EXPECT_TRUE(
      RunReadLogSourceFunction(params, &result_reader_id, &result_string));
}

}  // namespace extensions
