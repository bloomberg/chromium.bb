// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/scanner_controller.h"

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/logging/registry_logger.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/fake_shortcut_parser.h"
#include "chrome/chrome_cleaner/pup_data/test_uws.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

using ::testing::UnorderedElementsAreArray;

constexpr UwSId kUnknownUwSID = 98765;

class StubScannerController : public ScannerController {
 public:
  StubScannerController(RegistryLogger* registry_logger,
                        ShortcutParserAPI* shortcut_parser)
      : ScannerController(registry_logger, shortcut_parser) {}
  ~StubScannerController() override = default;

  void SetResult(ResultCode status) { status_ = status; }

  void SetWatchdogWillTimeout() { watchdog_timeout_ = true; }

  void SetFoundUwS(const std::vector<UwSId>& found_uws) {
    found_uws_ = found_uws;
  }

  ResultCode watchdog_result() const { return watchdog_result_; }

 protected:
  void StartScan() override {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunnerHandle::Get();
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&StubScannerController::UpdateScanResults,
                                  base::Unretained(this), found_uws_));

    if (watchdog_timeout_) {
      base::PostTaskAndReplyWithResult(
          task_runner.get(), FROM_HERE,
          base::BindOnce(&StubScannerController::WatchdogTimeoutCallback,
                         base::Unretained(this)),
          base::BindOnce(&StubScannerController::SetWatchdogResult,
                         base::Unretained(this)));

      // In the real app, the watchdog slays the current process after calling
      // WatchdogTimeoutCallback. Instead break out of the current test.
      task_runner->PostTask(FROM_HERE, QuitClosureForTesting());
      return;
    }

    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&StubScannerController::DoneScanning,
                                  base::Unretained(this), status_, found_uws_));
  }

  // In the real app the watchdog slays the current process after calling
  // WatchdogTimeoutCallback, so ScanOnly() never returns with the result code.
  // Instead store it in a separate member variable that can be checked by the
  // test.
  void SetWatchdogResult(ResultCode watchdog_result) {
    watchdog_result_ = watchdog_result;
  }

 private:
  ResultCode status_ = RESULT_CODE_FAILED;
  std::vector<UwSId> found_uws_;
  bool watchdog_timeout_ = false;

  ResultCode watchdog_result_ = RESULT_CODE_FAILED;
};

class ScannerControllerTest : public testing::Test {
 public:
  ScannerControllerTest() {
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    // The registry logger must be created after calling OverrideRegistry.
    registry_logger_ =
        std::make_unique<RegistryLogger>(RegistryLogger::Mode::REPORTER);
  }

  void ExpectFoundUwS(const std::vector<UwSId> found_uws) {
    std::vector<std::wstring> found_uws_strings;
    for (const UwSId uws_id : found_uws) {
      found_uws_strings.push_back(base::NumberToString16(uws_id));
    }

    base::win::RegKey logging_key(HKEY_CURRENT_USER);
    ASSERT_EQ(
        logging_key.OpenKey(kSoftwareRemovalToolRegistryKey, KEY_QUERY_VALUE),
        ERROR_SUCCESS);

    std::vector<std::wstring> found_uws_value;
    ASSERT_EQ(logging_key.ReadValues(kFoundUwsValueName, &found_uws_value),
              ERROR_SUCCESS);
    EXPECT_THAT(found_uws_value, UnorderedElementsAreArray(found_uws_strings));
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  registry_util::RegistryOverrideManager registry_override_manager_;
  std::unique_ptr<RegistryLogger> registry_logger_;

  FakeShortcutParser shortcut_parser_;
};

TEST_F(ScannerControllerTest, NothingFound) {
  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetResult(RESULT_CODE_SUCCESS);

  EXPECT_EQ(controller.ScanOnly(), RESULT_CODE_NO_PUPS_FOUND);
  ExpectFoundUwS({});
}

TEST_F(ScannerControllerTest, ReportOnlyUwSFound) {
  const std::vector<UwSId> kFoundUwS{kGoogleTestAUwSID};

  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetResult(RESULT_CODE_SUCCESS);
  controller.SetFoundUwS(kFoundUwS);

  EXPECT_EQ(controller.ScanOnly(), RESULT_CODE_REPORT_ONLY_PUPS_FOUND);
  ExpectFoundUwS(kFoundUwS);
}

TEST_F(ScannerControllerTest, CleanableUwSFound) {
  // GoogleTestB is marked cleanable.
  const std::vector<UwSId> kFoundUwS{kGoogleTestAUwSID, kGoogleTestBUwSID};

  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetResult(RESULT_CODE_SUCCESS);
  controller.SetFoundUwS(kFoundUwS);

  EXPECT_EQ(controller.ScanOnly(), RESULT_CODE_SUCCESS);
  ExpectFoundUwS(kFoundUwS);
}

TEST_F(ScannerControllerTest, ErrorWithUwSFound) {
  const std::vector<UwSId> kFoundUwS{kGoogleTestAUwSID, kGoogleTestBUwSID};

  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetResult(RESULT_CODE_FAILED);
  controller.SetFoundUwS(kFoundUwS);

  EXPECT_EQ(controller.ScanOnly(), RESULT_CODE_FAILED);
  ExpectFoundUwS(kFoundUwS);
}

TEST_F(ScannerControllerTest, UnknownUwSFound) {
  const std::vector<UwSId> kFoundUwS{kGoogleTestAUwSID, kGoogleTestBUwSID,
                                     kUnknownUwSID};

  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetResult(RESULT_CODE_SUCCESS);
  controller.SetFoundUwS(kFoundUwS);

  EXPECT_EQ(controller.ScanOnly(), RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS);
  ExpectFoundUwS(kFoundUwS);
}

TEST_F(ScannerControllerTest, TimeoutWithNothingFound) {
  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetWatchdogWillTimeout();

  // The return value of ScanOnly isn't used when the watchdog slays the
  // process.
  controller.ScanOnly();
  EXPECT_EQ(controller.watchdog_result(),
            RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS);
  ExpectFoundUwS({});
}

TEST_F(ScannerControllerTest, TimeoutWithReportOnlyUwSFound) {
  const std::vector<UwSId> kFoundUwS{kGoogleTestAUwSID};

  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetWatchdogWillTimeout();
  controller.SetFoundUwS(kFoundUwS);

  // The return value of ScanOnly isn't used when the watchdog slays the
  // process.
  controller.ScanOnly();
  EXPECT_EQ(controller.watchdog_result(),
            RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS);
  ExpectFoundUwS(kFoundUwS);
}

TEST_F(ScannerControllerTest, TimeoutWithCleanableUwSFound) {
  // GoogleTestB is marked cleanable.
  const std::vector<UwSId> kFoundUwS{kGoogleTestAUwSID, kGoogleTestBUwSID};

  StubScannerController controller(registry_logger_.get(), &shortcut_parser_);
  controller.SetWatchdogWillTimeout();
  controller.SetFoundUwS(kFoundUwS);

  // The return value of ScanOnly isn't used when the watchdog slays the
  // process.
  controller.ScanOnly();
  EXPECT_EQ(controller.watchdog_result(),
            RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS);
  ExpectFoundUwS(kFoundUwS);
}

}  // namespace

}  // namespace chrome_cleaner
