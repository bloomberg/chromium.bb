// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_common.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test_utils.h"

namespace test {

// Relative to the chrome/test/data directory.
const base::FilePath::CharType kReferenceFilesDirName[] =
    FILE_PATH_LITERAL("webrtc/resources");
const base::FilePath::CharType kReferenceFileName360p[] =
    FILE_PATH_LITERAL("reference_video_640x360_30fps");
const base::FilePath::CharType kReferenceFileName720p[] =
    FILE_PATH_LITERAL("reference_video_1280x720_30fps");
const base::FilePath::CharType kYuvFileExtension[] = FILE_PATH_LITERAL("yuv");
const base::FilePath::CharType kY4mFileExtension[] = FILE_PATH_LITERAL("y4m");

// This message describes how to modify your .gclient to get the reference
// video files downloaded for you.
static const char kAdviseOnGclientSolution[] =
    "You need to add this solution to your .gclient to run this test:\n"
    "{\n"
    "  \"name\"        : \"webrtc.DEPS\",\n"
    "  \"url\"         : \"svn://svn.chromium.org/chrome/trunk/deps/"
    "third_party/webrtc/webrtc.DEPS\",\n"
    "}";

const int kDefaultPollIntervalMsec = 250;

base::FilePath GetReferenceFilesDir() {
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);

  return test_data_dir.Append(kReferenceFilesDirName);
}

bool HasReferenceFilesInCheckout() {
  if (!base::PathExists(GetReferenceFilesDir())) {
    LOG(ERROR)
        << "Cannot find the working directory for the reference video "
        << "files, expected at " << GetReferenceFilesDir().value() << ". " <<
        kAdviseOnGclientSolution;
    return false;
  }
  return HasYuvAndY4mFile(test::kReferenceFileName360p) &&
      HasYuvAndY4mFile(test::kReferenceFileName720p);
}

bool HasYuvAndY4mFile(const base::FilePath::CharType* reference_file) {
  base::FilePath webrtc_reference_video_yuv = GetReferenceFilesDir()
      .Append(reference_file).AddExtension(kYuvFileExtension);
  if (!base::PathExists(webrtc_reference_video_yuv)) {
    LOG(ERROR)
        << "Missing YUV reference video to be used for quality"
        << " comparison, expected at " << webrtc_reference_video_yuv.value()
        << ". " << kAdviseOnGclientSolution;
    return false;
  }

  base::FilePath webrtc_reference_video_y4m = GetReferenceFilesDir()
      .Append(reference_file).AddExtension(kY4mFileExtension);
  if (!base::PathExists(webrtc_reference_video_y4m)) {
    LOG(ERROR)
        << "Missing Y4M reference video to be used for quality"
        << " comparison, expected at "<< webrtc_reference_video_y4m.value()
        << ". " << kAdviseOnGclientSolution;
    return false;
  }
  return true;
}

bool SleepInJavascript(content::WebContents* tab_contents, int timeout_msec) {
  const std::string javascript = base::StringPrintf(
      "setTimeout(function() {"
      "  window.domAutomationController.send('sleep-ok');"
      "}, %d)", timeout_msec);

  std::string result;
  bool ok = content::ExecuteScriptAndExtractString(
      tab_contents, javascript, &result);
  return ok && result == "sleep-ok";
}

bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents) {
  return PollingWaitUntil(javascript, evaluates_to, tab_contents,
                          kDefaultPollIntervalMsec);
}

bool PollingWaitUntil(const std::string& javascript,
                      const std::string& evaluates_to,
                      content::WebContents* tab_contents,
                      int poll_interval_msec) {
  base::Time start_time = base::Time::Now();
  base::TimeDelta timeout = TestTimeouts::action_max_timeout();
  std::string result;

  while (base::Time::Now() - start_time < timeout) {
    std::string result;
    if (!content::ExecuteScriptAndExtractString(tab_contents, javascript,
                                                &result)) {
      LOG(ERROR) << "Failed to execute javascript " << javascript;
      return false;
    }

    if (evaluates_to == result)
      return true;

    // Sleep a bit here to keep this loop from spinlocking too badly.
    if (!SleepInJavascript(tab_contents, poll_interval_msec)) {
      // TODO(phoglund): Figure out why this fails every now and then.
      // It's not a huge deal if it does though.
      LOG(ERROR) << "Failed to sleep.";
    }
  }
  LOG(ERROR)
      << "Timed out while waiting for " << javascript
      << " to evaluate to " << evaluates_to << "; last result was '" << result
      << "'";
  return false;
}

}  // namespace test
