// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/log_source_access_manager.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/feedback_private/log_source_resource.h"
#include "chrome/browser/extensions/api/feedback_private/single_log_source_factory.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "extensions/browser/api/api_resource_manager.h"

namespace extensions {

namespace {

using api::feedback_private::LOG_SOURCE_MESSAGES;
using api::feedback_private::LOG_SOURCE_UILATEST;
using api::feedback_private::ReadLogSourceResult;
using api::feedback_private::ReadLogSourceParams;
using system_logs::SingleLogSource;
using SupportedSource = system_logs::SingleLogSource::SupportedSource;

std::unique_ptr<KeyedService> ApiResourceManagerTestFactory(
    content::BrowserContext* context) {
  return base::MakeUnique<ApiResourceManager<LogSourceResource>>(context);
}

// Dummy function used as a callback for FetchFromSource().
void OnFetchedFromSource(const ReadLogSourceResult& result) {}

// A dummy SingleLogSource that does not require real system logs to be
// available during testing. Always returns an empty result.
class EmptySingleLogSource : public SingleLogSource {
 public:
  explicit EmptySingleLogSource(SupportedSource type) : SingleLogSource(type) {}

  ~EmptySingleLogSource() override = default;

  void Fetch(const system_logs::SysLogsSourceCallback& callback) override {
    system_logs::SystemLogsResponse* result_map =
        new system_logs::SystemLogsResponse;
    result_map->emplace("", "");

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
    return base::MakeUnique<EmptySingleLogSource>(type);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptySingleLogSource);
};

}  // namespace

class LogSourceAccessManagerTest : public ExtensionApiUnittest {
 public:
  LogSourceAccessManagerTest()
      : create_callback_(base::Bind(&EmptySingleLogSource::Create)) {}
  ~LogSourceAccessManagerTest() override {}

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

    ExtensionApiUnittest::TearDown();
  }

 private:
  // Passed to SingleLogSourceFactory so that the API can create an instance of
  // TestSingleLogSource for testing.
  SingleLogSourceFactory::CreateCallback create_callback_;

  DISALLOW_COPY_AND_ASSIGN(LogSourceAccessManagerTest);
};

TEST_F(LogSourceAccessManagerTest, MaxNumberOfOpenLogSources) {
  const base::TimeDelta timeout(base::TimeDelta::FromMilliseconds(0));
  LogSourceAccessManager::SetRateLimitingTimeoutForTesting(&timeout);

  LogSourceAccessManager manager(profile());
  LogSourceAccessManager::ReadLogSourceCallback callback =
      base::Bind(&OnFetchedFromSource);

  int count = 0;

  // Open 10 readers for LOG_SOURCE_MESSAGES.
  ReadLogSourceParams messages_params;
  messages_params.incremental = false;
  messages_params.source = LOG_SOURCE_MESSAGES;
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_TRUE(manager.FetchFromSource(
        messages_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(0U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Open 10 readers for LOG_SOURCE_UILATEST.
  ReadLogSourceParams ui_latest_params;
  ui_latest_params.incremental = false;
  ui_latest_params.source = LOG_SOURCE_UILATEST;
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_TRUE(manager.FetchFromSource(
        ui_latest_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Can't open more readers for LOG_SOURCE_MESSAGES.
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_FALSE(manager.FetchFromSource(
        messages_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Can't open more readers for LOG_SOURCE_UILATEST.
  for (int i = 0; i < 10; ++i, ++count) {
    EXPECT_FALSE(manager.FetchFromSource(
        ui_latest_params, base::StringPrintf("extension %d", count), callback))
        << count;
  }
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_MESSAGES));
  EXPECT_EQ(10U, manager.GetNumActiveResourcesForSource(LOG_SOURCE_UILATEST));

  // Wait for all asynchronous operations to complete.
  base::RunLoop().RunUntilIdle();
}

}  // namespace extensions
