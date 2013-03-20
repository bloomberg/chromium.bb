// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test scrubber uses a Google Test event listener to trigger scrubs.
// Scrubs are performed at the start of the test program and at the conclusion
// of each test.

#include "chrome_frame/test/test_scrubber.h"

#include <windows.h>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/test/test_process_killer_win.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_frame_test {

namespace {

class TestScrubber {
 public:
  TestScrubber();
  ~TestScrubber();

  // Initializes the instance, causing it to provide its services for all
  // subsequent tests.
  void Initialize(testing::UnitTest* unit_test);

  // Sets the user data directory for the current test.
  void set_data_directory_override(const base::StringPiece16& user_data_dir) {
    user_data_dir.as_string().swap(data_directory_override_);
  }

  // Kills all instances of IE and all instances of chrome.exe that have
  // --chrome-frame on their command-lines.  Also deletes the user data
  // directory used by the test.  Ordinarily, this is the default Chrome Frame
  // user data directory for IE.  Individual tests that use a specific directory
  // can override this via |set_data_directory_override|.
  void CleanUpFromTestRun();

 private:
  // A listener that calls back to the scrubber at the start of the test program
  // and at the end of each test.
  class EventListener : public testing::EmptyTestEventListener {
   public:
    explicit EventListener(TestScrubber* scrubber) : scrubber_(scrubber) {
    }

    virtual void OnTestProgramStart(const testing::UnitTest&) OVERRIDE {
      scrubber_->CleanUpFromTestRun();
    }

    virtual void OnTestEnd(const testing::TestInfo&) OVERRIDE {
      scrubber_->CleanUpFromTestRun();
    }

   private:
    TestScrubber* scrubber_;
    DISALLOW_COPY_AND_ASSIGN(EventListener);
  };

  bool is_initialized() const { return !default_data_directory_.empty(); }

  string16 default_data_directory_;
  string16 data_directory_override_;

  DISALLOW_COPY_AND_ASSIGN(TestScrubber);
};

TestScrubber::TestScrubber() {
}

TestScrubber::~TestScrubber() {
}

void TestScrubber::Initialize(testing::UnitTest* unit_test) {
  DCHECK(!is_initialized());

  default_data_directory_ = GetProfilePathForIE().value();
  data_directory_override_.clear();
  unit_test->listeners().Append(new EventListener(this));
}

void TestScrubber::CleanUpFromTestRun() {
  // Kill all iexplore.exe and ieuser.exe processes.
  base::KillProcesses(chrome_frame_test::kIEImageName, 0, NULL);
  base::KillProcesses(chrome_frame_test::kIEBrokerImageName, 0, NULL);

  // Kill all chrome_launcher processes trying to launch Chrome.
  base::KillProcesses(chrome_frame_test::kChromeLauncher, 0, NULL);

  // Kill all chrome.exe processes with --chrome-frame.
  base::KillAllNamedProcessesWithArgument(
      chrome::kBrowserProcessExecutableName,
      ASCIIToWide(switches::kChromeFrame));

  // Delete the user data directory.
  base::FilePath data_directory(data_directory_override_.empty() ?
                          default_data_directory_ :
                          data_directory_override_);

  VLOG_IF(1, file_util::PathExists(data_directory))
      << __FUNCTION__ << " deleting user data directory "
      << data_directory.value();
  bool deleted = file_util::Delete(data_directory, true);
  LOG_IF(ERROR, !deleted)
      << "Failed to delete user data directory directory "
      << data_directory.value();

  // Clear the overridden data directory for the next test.
  data_directory_override_.clear();
}

base::LazyInstance<TestScrubber> g_scrubber = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void InstallTestScrubber(testing::UnitTest* unit_test) {
  // Must be called before running any tests.
  DCHECK(unit_test);
  DCHECK(!unit_test->current_test_case());

  g_scrubber.Get().Initialize(unit_test);
}

void OverrideDataDirectoryForThisTest(
    const base::StringPiece16& user_data_dir) {
  // Must be called within the context of a test.
  DCHECK(testing::UnitTest::GetInstance()->current_test_info());

  g_scrubber.Get().set_data_directory_override(user_data_dir);
}

}  // namespace chrome_frame_test
