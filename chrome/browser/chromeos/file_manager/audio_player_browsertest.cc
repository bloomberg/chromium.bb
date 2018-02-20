// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

namespace file_manager {

template <GuestMode M>
class AudioPlayerBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return M; }
  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

 protected:
  const char* GetTestManifestName() const override {
    return "audio_player_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;
};

typedef AudioPlayerBrowserTestBase<NOT_IN_GUEST_MODE> AudioPlayerBrowserTest;
typedef AudioPlayerBrowserTestBase<IN_GUEST_MODE>
    AudioPlayerBrowserTestInGuestMode;

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenAudioOnDownloads DISABLED_OpenAudioOnDownloads
#else
// TODO(yamaguchi): Enable after removing root cause of the test flakiness.
// http://crbug.com/804413.
#define MAYBE_OpenAudioOnDownloads DISABLED_OpenAudioOnDownloads
#endif
IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, MAYBE_OpenAudioOnDownloads) {
  set_test_case_name("openAudioOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTestInGuestMode,
                       OpenAudioOnDownloads) {
  set_test_case_name("openAudioOnDownloads");
  StartTest();
}

// http://crbug.com/508949
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenAudioOnDrive DISABLED_OpenAudioOnDrive
#else
// TODO(yamaguchi): Enable after removing root cause of the test flakiness.
// http://crbug.com/804413.
#define MAYBE_OpenAudioOnDrive DISABLED_OpenAudioOnDrive
#endif
IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, MAYBE_OpenAudioOnDrive) {
  set_test_case_name("openAudioOnDrive");
  StartTest();
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_TogglePlayState DISABLED_TogglePlayState
#else
// TODO(yamaguchi): Enable after removing root cause of the test flakiness.
// http://crbug.com/804413.
#define MAYBE_TogglePlayState DISABLED_TogglePlayState
#endif
IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, MAYBE_TogglePlayState) {
  set_test_case_name("togglePlayState");
  StartTest();
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_ChangeVolumeLevel DISABLED_ChangeVolumeLevel
#else
// TODO(yamaguchi): Enable after removing root cause of the test flakiness.
// http://crbug.com/804413.
#define MAYBE_ChangeVolumeLevel DISABLED_ChangeVolumeLevel
#endif
IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, MAYBE_ChangeVolumeLevel) {
  set_test_case_name("changeVolumeLevel");
  StartTest();
}

#if defined(MEMORY_SANITIZER)
#define MAYBE_ChangeTracks DISABLED_ChangeTracks
#else
// Also disable because of flakyness. http://crbug.com/618198
#define MAYBE_ChangeTracks DISABLED_ChangeTracks
#endif
IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, MAYBE_ChangeTracks) {
  set_test_case_name("changeTracks");
  StartTest();
}

}  // namespace file_manager
