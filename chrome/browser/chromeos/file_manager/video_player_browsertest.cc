// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

#include "chromeos/chromeos_switches.h"

namespace file_manager {

template <GuestMode M>
class VideoPlayerBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return M; }
  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        chromeos::switches::kEnableVideoPlayerChromecastSupport);
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
  }

  const char* GetTestManifestName() const override {
    return "video_player_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;
};

typedef VideoPlayerBrowserTestBase<NOT_IN_GUEST_MODE> VideoPlayerBrowserTest;
typedef VideoPlayerBrowserTestBase<IN_GUEST_MODE>
    VideoPlayerBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, OpenSingleVideoOnDownloads) {
  set_test_case_name("openSingleVideoOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenSingleVideoOnDownloads DISABLED_OpenSingleVideoOnDownloads
#else
#define MAYBE_OpenSingleVideoOnDownloads OpenSingleVideoOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTestInGuestMode,
                       MAYBE_OpenSingleVideoOnDownloads) {
  set_test_case_name("openSingleVideoOnDownloads");
  StartTest();
}

// MEMORY_SANITIZER: http://crbug.com/508949
// CHROME_OS: http://crbug.com/688568
#if defined(MEMORY_SANITIZER) || defined(OS_CHROMEOS)
#define MAYBE_OpenSingleVideoOnDrive DISABLED_OpenSingleVideoOnDrive
#else
#define MAYBE_OpenSingleVideoOnDrive OpenSingleVideoOnDrive
#endif
IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, MAYBE_OpenSingleVideoOnDrive) {
  set_test_case_name("openSingleVideoOnDrive");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_CheckInitialElements DISABLED_CheckInitialElements
#else
#define MAYBE_CheckInitialElements CheckInitialElements
#endif
IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, MAYBE_CheckInitialElements) {
  set_test_case_name("checkInitialElements");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_ClickControlButtons DISABLED_ClickControlButtons
#else
#define MAYBE_ClickControlButtons ClickControlButtons
#endif
IN_PROC_BROWSER_TEST_F(VideoPlayerBrowserTest, MAYBE_ClickControlButtons) {
  set_test_case_name("clickControlButtons");
  StartTest();
}

}  // namespace file_manager
