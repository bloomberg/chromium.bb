// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
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
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/python_utils.h"
#include "ui/gl/gl_switches.h"

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
// test suite since normal tester machines do not have webcams. Chrome will use
// its fake camera for both tests, but Firefox will use the real webcam in the
// Firefox interop test. Thus, this test must on a machine with a real webcam.
//
// This test will bring up a AppRTC instance on localhost and verify that the
// call gets up when connecting to the same room from two tabs in a browser.
class WebRtcApprtcBrowserTest : public WebRtcTestBase {
 public:
  WebRtcApprtcBrowserTest()
      : dev_appserver_(base::kNullProcessHandle),
        firefox_(base::kNullProcessHandle) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  virtual void TearDown() OVERRIDE {
    // Kill any processes we may have brought up.
    LOG(INFO) << "Entering TearDown";
    if (dev_appserver_ != base::kNullProcessHandle)
      base::KillProcess(dev_appserver_, 0, false);
    // TODO(phoglund): Find some way to shut down Firefox cleanly on Windows.
    if (firefox_ != base::kNullProcessHandle)
      base::KillProcess(firefox_, 0, false);
    LOG(INFO) << "Exiting TearDown";
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

    VLOG(1) << "Running " << command_line.GetCommandLineString();
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

  bool WaitForCallToComeUp(content::WebContents* tab_contents) {
    // Apprtc will set remoteVideo.style.opacity to 1 when the call comes up.
    std::string javascript =
        "window.domAutomationController.send(remoteVideo.style.opacity)";
    return test::PollingWaitUntil(javascript, "1", tab_contents);
  }

  bool WaitForCallToHangUp(content::WebContents* tab_contents) {
    // Apprtc will set remoteVideo.style.opacity to 1 when the call comes up.
    std::string javascript =
        "window.domAutomationController.send(remoteVideo.style.opacity)";
    return test::PollingWaitUntil(javascript, "0", tab_contents);
  }

  bool EvalInJavascriptFile(content::WebContents* tab_contents,
                            const base::FilePath& path) {
    std::string javascript;
    if (!ReadFileToString(path, &javascript)) {
      LOG(ERROR) << "Missing javascript code at " << path.value() << ".";
      return false;
    }

    if (!content::ExecuteScript(tab_contents, javascript)) {
      LOG(ERROR) << "Failed to execute the following javascript: " <<
          javascript;
      return false;
    }
    return true;
  }

  bool DetectRemoteVideoPlaying(content::WebContents* tab_contents) {
    if (!EvalInJavascriptFile(tab_contents, GetSourceDir().Append(
        FILE_PATH_LITERAL("chrome/test/data/webrtc/test_functions.js"))))
      return false;
    if (!EvalInJavascriptFile(tab_contents, GetSourceDir().Append(
        FILE_PATH_LITERAL("chrome/test/data/webrtc/video_detector.js"))))
      return false;

    // The remote video tag is called remoteVideo in the AppRTC code.
    StartDetectingVideo(tab_contents, "remoteVideo");
    WaitForVideoToPlay(tab_contents);
    return true;
  }

  bool HangUpApprtcCall(content::WebContents* tab_contents) {
    // This is the same as clicking the Hangup button in the AppRTC call.
    return content::ExecuteScript(tab_contents, "onHangup()");
  }

  base::FilePath GetSourceDir() {
    base::FilePath source_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &source_dir);
    return source_dir;
  }

  bool LaunchFirefoxWithUrl(const GURL& url) {
    base::FilePath firefox_binary =
        GetSourceDir().Append(
            FILE_PATH_LITERAL("../firefox-nightly/firefox/firefox"));
    if (!base::PathExists(firefox_binary)) {
      LOG(ERROR) << "Missing firefox binary at " <<
          firefox_binary.value() << ". " << kAdviseOnGclientSolution;
      return false;
    }
    base::FilePath firefox_launcher =
        GetSourceDir().Append(
            FILE_PATH_LITERAL("../webrtc.DEPS/run_firefox_webrtc.py"));
    if (!base::PathExists(firefox_launcher)) {
      LOG(ERROR) << "Missing firefox launcher at " <<
          firefox_launcher.value() << ". " << kAdviseOnGclientSolution;
      return false;
    }

    CommandLine command_line(firefox_launcher);
    command_line.AppendSwitchPath("--binary", firefox_binary);
    command_line.AppendSwitchASCII("--webpage", url.spec());

    VLOG(1) << "Running " << command_line.GetCommandLineString();
    return base::LaunchProcess(command_line, base::LaunchOptions(),
                               &firefox_);
  }

  bool HasWebcamOnSystem() {
#if defined(OS_LINUX)
    // Implementation note: normally we would be able to figure this out with
    // MediaStreamTrack.getSources, but we can't ask Chrome since it runs in
    // fake device mode where it will not enumerate webcams on the system.
    // Therefore, look for /dev/video* entries directly since this test only
    // runs on Linux for now anyway.
    base::FileEnumerator dev_video(base::FilePath(FILE_PATH_LITERAL("/dev")),
                                   false, base::FileEnumerator::FILES,
                                   FILE_PATH_LITERAL("video*"));
    return !dev_video.Next().empty();
#endif
    NOTREACHED();
    return false;
  }

 private:
  base::ProcessHandle dev_appserver_;
  base::ProcessHandle firefox_;
};

IN_PROC_BROWSER_TEST_F(WebRtcApprtcBrowserTest, MANUAL_WorksOnApprtc) {
  // Disabled on Win XP: http://code.google.com/p/webrtc/issues/detail?id=2703.
  if (OnWinXp())
    return;

  DetectErrorsInJavaScript();
  ASSERT_TRUE(LaunchApprtcInstanceOnLocalhost());
  while (!LocalApprtcInstanceIsUp())
    VLOG(1) << "Waiting for AppRTC to come up...";

  GURL room_url = GURL(base::StringPrintf("localhost:9999?r=room_%d",
                                          base::RandInt(0, 65536)));

  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* left_tab = OpenPageAndAcceptUserMedia(room_url);

  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* right_tab = OpenPageAndAcceptUserMedia(room_url);

  ASSERT_TRUE(WaitForCallToComeUp(left_tab));
  ASSERT_TRUE(WaitForCallToComeUp(right_tab));

  ASSERT_TRUE(DetectRemoteVideoPlaying(left_tab));
  ASSERT_TRUE(DetectRemoteVideoPlaying(right_tab));

  ASSERT_TRUE(HangUpApprtcCall(left_tab));

  ASSERT_TRUE(WaitForCallToHangUp(left_tab));
  ASSERT_TRUE(WaitForCallToHangUp(right_tab));

  chrome::CloseWebContents(browser(), left_tab, false);
  chrome::CloseWebContents(browser(), right_tab, false);
}

#if defined(OS_LINUX)
#define MAYBE_MANUAL_FirefoxApprtcInteropTest MANUAL_FirefoxApprtcInteropTest
#else
// Not implemented yet on Windows and Mac.
#define MAYBE_MANUAL_FirefoxApprtcInteropTest DISABLED_MANUAL_FirefoxApprtcInteropTest
#endif

IN_PROC_BROWSER_TEST_F(WebRtcApprtcBrowserTest,
                       MAYBE_MANUAL_FirefoxApprtcInteropTest) {
  // Disabled on Win XP: http://code.google.com/p/webrtc/issues/detail?id=2703.
  if (OnWinXp())
    return;

  if (!HasWebcamOnSystem()) {
    LOG(INFO)
        << "Didn't find a webcam on the system; skipping test since Firefox "
        << "needs to be able to acquire a webcam.";
    return;
  }

  DetectErrorsInJavaScript();
  ASSERT_TRUE(LaunchApprtcInstanceOnLocalhost());
  while (!LocalApprtcInstanceIsUp())
    VLOG(1) << "Waiting for AppRTC to come up...";

  GURL room_url = GURL(base::StringPrintf("http://localhost:9999?r=room_%d",
                                          base::RandInt(0, 65536)));
  content::WebContents* chrome_tab = OpenPageAndAcceptUserMedia(room_url);

  ASSERT_TRUE(LaunchFirefoxWithUrl(room_url));

  ASSERT_TRUE(WaitForCallToComeUp(chrome_tab));

  // Ensure Firefox manages to send video our way.
  ASSERT_TRUE(DetectRemoteVideoPlaying(chrome_tab));
}
