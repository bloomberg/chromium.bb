// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"

namespace file_manager {

template <GuestMode MODE>
class AudioPlayerBrowserTestBase : public FileManagerBrowserTestBase {
 public:
  GuestMode GetGuestModeParam() const override { return MODE; }

  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

  const char* GetTestExtensionManifestName() const override {
    return "audio_player_test_manifest.json";
  }

 protected:
  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;
};

typedef AudioPlayerBrowserTestBase<NOT_IN_GUEST_MODE> AudioPlayerBrowserTest;
typedef AudioPlayerBrowserTestBase<IN_GUEST_MODE>
    AudioPlayerBrowserTestInGuestMode;

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, OpenAudioOnDownloads) {
  set_test_case_name("openAudioOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTestInGuestMode,
                       OpenAudioOnDownloads) {
  set_test_case_name("openAudioOnDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, OpenAudioOnDrive) {
  set_test_case_name("openAudioOnDrive");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, TogglePlayState) {
  set_test_case_name("togglePlayState");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, ChangeVolumeLevel) {
  set_test_case_name("changeVolumeLevel");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(AudioPlayerBrowserTest, ChangeTracks) {
  set_test_case_name("changeTracks");
  StartTest();
}

}  // namespace file_manager
