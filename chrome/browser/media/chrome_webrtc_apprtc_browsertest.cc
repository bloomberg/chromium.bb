// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/python_utils.h"


// You need this solution to run this test. The solution will download appengine
// and the apprtc code for you.
const char kAdviseOnGclientSolution[] =
    "You need to add this solution to your .gclient to run this test:\n"
    "{\n"
    "  \"name\"        : \"webrtc.DEPS\",\n"
    "  \"url\"         : \"svn://svn.chromium.org/chrome/trunk/deps/"
    "third_party/webrtc/webrtc.DEPS\",\n"
    "}";
const char kTitlePageOfAppEngineAdminPage[] = "Instances";


// WebRTC-AppRTC integration test. Requires a real webcam and microphone
// on the running system. This test is not meant to run in the main browser
// test suite since normal tester machines do not have webcams.
//
// This test will bring up a AppRTC instance on localhost and verify that the
// call gets up when connecting to the same room from two tabs in a browser.
class WebrtcApprtcBrowserTest : public WebRtcTestBase {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

 protected:
  bool LaunchApprtcInstanceOnLocalhost() {
    base::FilePath appengine_dev_appserver =
        GetSourceDir().Append(
            FILE_PATH_LITERAL("../google_appengine/dev_appserver.py"));
    if (!base::PathExists(appengine_dev_appserver)) {
      LOG(ERROR) << "Missing appengine sdk at " <<
          appengine_dev_appserver.value() << ". " << kAdviseOnGclientSolution;
      return false;
    }

    base::FilePath apprtc_dir =
        GetSourceDir().Append(FILE_PATH_LITERAL("out/apprtc"));
    if (!base::PathExists(apprtc_dir)) {
      LOG(ERROR) << "Missing AppRTC code at " <<
          apprtc_dir.value() << ". " << kAdviseOnGclientSolution;
      return false;
    }

    CommandLine command_line(CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&command_line));

    command_line.AppendArgPath(appengine_dev_appserver);
    command_line.AppendArgPath(apprtc_dir);
    command_line.AppendArg("--port=9999");
    command_line.AppendArg("--admin_port=9998");
    command_line.AppendArg("--skip_sdk_update_check");

    LOG(INFO) << "Running " << command_line.GetCommandLineString();
    return base::LaunchProcess(command_line, base::LaunchOptions(),
                               &dev_appserver_);
  }

  bool LocalApprtcInstanceIsUp() {
    // Load the admin page and see if we manage to load it right.
    ui_test_utils::NavigateToURL(browser(), GURL("localhost:9998"));
    content::WebContents* tab_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string javascript =
        "window.domAutomationController.send(document.title)";
    std::string result;
    if (!content::ExecuteScriptAndExtractString(tab_contents, javascript,
                                                &result))
      return false;

    return result == kTitlePageOfAppEngineAdminPage;
  }

  bool StopApprtcInstance() {
    return base::KillProcess(dev_appserver_, 0, false);
  }

  bool WaitForCallToComeUp(content::WebContents* tab_contents) {
    // Apprtc will set remoteVideo.style.opacity to 1 when the call comes up.
    std::string javascript =
        "window.domAutomationController.send(remoteVideo.style.opacity)";
    return PollingWaitUntil(javascript, "1", tab_contents);
  }

  base::FilePath GetSourceDir() {
    base::FilePath source_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &source_dir);
    return source_dir;
  }

 private:
  base::ProcessHandle dev_appserver_;
};

IN_PROC_BROWSER_TEST_F(WebrtcApprtcBrowserTest, MANUAL_WorksOnApprtc) {
  ASSERT_TRUE(LaunchApprtcInstanceOnLocalhost());
  while (!LocalApprtcInstanceIsUp())
    LOG(INFO) << "Waiting for AppRTC to come up...";

  GURL room_url = GURL(base::StringPrintf("localhost:9999?r=room_%d",
                                          base::RandInt(0, 65536)));

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* left_tab = OpenPageAndAcceptUserMedia(room_url);
  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab = OpenPageAndAcceptUserMedia(room_url);

  ASSERT_TRUE(WaitForCallToComeUp(left_tab));
  ASSERT_TRUE(WaitForCallToComeUp(right_tab));

  ASSERT_TRUE(StopApprtcInstance());
}
