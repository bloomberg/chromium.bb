// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_responder.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/python_utils.h"
#include "ui/gl/gl_switches.h"

const char kTitlePageOfAppEngineAdminPage[] = "Instances";

const char kIsApprtcCallUpJavascript[] =
    "var remoteVideo = document.querySelector('#remote-video');"
    "var remoteVideoActive ="
    "    remoteVideo != null &&"
    "    remoteVideo.classList.contains('active');"
    "window.domAutomationController.send(remoteVideoActive.toString());";


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
  WebRtcApprtcBrowserTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
  }

  void TearDown() override {
    // Kill any processes we may have brought up. Note: this isn't perfect,
    // especially if the test hangs or if we're on Windows.
    LOG(INFO) << "Entering TearDown";
    if (dev_appserver_.IsValid())
      dev_appserver_.Terminate(0, false);
    if (collider_server_.IsValid())
      collider_server_.Terminate(0, false);
    if (firefox_.IsValid())
      firefox_.Terminate(0, false);
    LOG(INFO) << "Exiting TearDown";
  }

 protected:
  bool LaunchApprtcInstanceOnLocalhost(const std::string& port) {
    base::FilePath appengine_dev_appserver =
        GetSourceDir().Append(
            FILE_PATH_LITERAL("../google_appengine/dev_appserver.py"));
    if (!base::PathExists(appengine_dev_appserver)) {
      LOG(ERROR) << "Missing appengine sdk at " <<
          appengine_dev_appserver.value() << ".\n" <<
          test::kAdviseOnGclientSolution;
      return false;
    }

    base::FilePath apprtc_dir =
        GetSourceDir().Append(FILE_PATH_LITERAL("out/apprtc/out/app_engine"));
    if (!base::PathExists(apprtc_dir)) {
      LOG(ERROR) << "Missing AppRTC AppEngine app at " <<
          apprtc_dir.value() << ".\n" << test::kAdviseOnGclientSolution;
      return false;
    }
    if (!base::PathExists(apprtc_dir.Append(FILE_PATH_LITERAL("app.yaml")))) {
      LOG(ERROR) << "The AppRTC AppEngine app at " <<
          apprtc_dir.value() << " appears to have not been built." <<
          "This should have been done by webrtc.DEPS scripts which invoke " <<
          "'grunt build' on AppRTC.";
      return false;
    }

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&command_line));

    command_line.AppendArgPath(appengine_dev_appserver);
    command_line.AppendArgPath(apprtc_dir);
    command_line.AppendArg("--port=" + port);
    command_line.AppendArg("--admin_port=9998");
    command_line.AppendArg("--skip_sdk_update_check");
    command_line.AppendArg("--clear_datastore=yes");

    DVLOG(1) << "Running " << command_line.GetCommandLineString();
    dev_appserver_ = base::LaunchProcess(command_line, base::LaunchOptions());
    return dev_appserver_.IsValid();
  }

  bool LaunchColliderOnLocalHost(const std::string& apprtc_url,
                                 const std::string& collider_port) {
    // The go workspace should be created, and collidermain built, at the
    // runhooks stage when webrtc.DEPS/build_apprtc_collider.py runs.
#if defined(OS_WIN)
    base::FilePath collider_server = GetSourceDir().Append(
        FILE_PATH_LITERAL("out/go-workspace/bin/collidermain.exe"));
#else
    base::FilePath collider_server = GetSourceDir().Append(
        FILE_PATH_LITERAL("out/go-workspace/bin/collidermain"));
#endif
    if (!base::PathExists(collider_server)) {
      LOG(ERROR) << "Missing Collider server binary at " <<
          collider_server.value() << ".\n" << test::kAdviseOnGclientSolution;
      return false;
    }

    base::CommandLine command_line(collider_server);

    command_line.AppendArg("-tls=false");
    command_line.AppendArg("-port=" + collider_port);
    command_line.AppendArg("-room-server=" + apprtc_url);

    DVLOG(1) << "Running " << command_line.GetCommandLineString();
    collider_server_ = base::LaunchProcess(command_line, base::LaunchOptions());
    return collider_server_.IsValid();
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
    return test::PollingWaitUntil(kIsApprtcCallUpJavascript, "true",
                                  tab_contents);
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
    StartDetectingVideo(tab_contents, "remote-video");
    WaitForVideoToPlay(tab_contents);
    return true;
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
          firefox_binary.value() << ".\n" << test::kAdviseOnGclientSolution;
      return false;
    }
    base::FilePath firefox_launcher =
        GetSourceDir().Append(
            FILE_PATH_LITERAL("../webrtc.DEPS/run_firefox_webrtc.py"));
    if (!base::PathExists(firefox_launcher)) {
      LOG(ERROR) << "Missing firefox launcher at " <<
          firefox_launcher.value() << ".\n" << test::kAdviseOnGclientSolution;
      return false;
    }

    base::CommandLine command_line(firefox_launcher);
    command_line.AppendSwitchPath("--binary", firefox_binary);
    command_line.AppendSwitchASCII("--webpage", url.spec());

    DVLOG(1) << "Running " << command_line.GetCommandLineString();
    firefox_ = base::LaunchProcess(command_line, base::LaunchOptions());
    return firefox_.IsValid();
  }

 private:
  base::Process dev_appserver_;
  base::Process firefox_;
  base::Process collider_server_;
};

IN_PROC_BROWSER_TEST_F(WebRtcApprtcBrowserTest, MANUAL_WorksOnApprtc) {
  DetectErrorsInJavaScript();
  ASSERT_TRUE(LaunchApprtcInstanceOnLocalhost("9999"));
  ASSERT_TRUE(LaunchColliderOnLocalHost("http://localhost:9999", "8089"));
  while (!LocalApprtcInstanceIsUp())
    DVLOG(1) << "Waiting for AppRTC to come up...";

  GURL room_url = GURL("http://localhost:9999/r/some_room"
                       "?wshpp=localhost:8089&wstls=false");

  // Set up the left tab.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  PermissionBubbleManager::FromWebContents(left_tab)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  InfoBarResponder left_infobar_responder(
      InfoBarService::FromWebContents(left_tab), InfoBarResponder::ACCEPT);
  ui_test_utils::NavigateToURL(browser(), room_url);

  // Set up the right tab.
  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  PermissionBubbleManager::FromWebContents(right_tab)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  InfoBarResponder right_infobar_responder(
      InfoBarService::FromWebContents(right_tab), InfoBarResponder::ACCEPT);
  ui_test_utils::NavigateToURL(browser(), room_url);

  ASSERT_TRUE(WaitForCallToComeUp(left_tab));
  ASSERT_TRUE(WaitForCallToComeUp(right_tab));

  ASSERT_TRUE(DetectRemoteVideoPlaying(left_tab));
  ASSERT_TRUE(DetectRemoteVideoPlaying(right_tab));

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
  DetectErrorsInJavaScript();
  ASSERT_TRUE(LaunchApprtcInstanceOnLocalhost("9999"));
  ASSERT_TRUE(LaunchColliderOnLocalHost("http://localhost:9999", "8089"));
  while (!LocalApprtcInstanceIsUp())
    DVLOG(1) << "Waiting for AppRTC to come up...";

  GURL room_url = GURL("http://localhost:9999/r/some_room"
                       "?wshpp=localhost:8089&wstls=false"
                       "&firefox_fake_device=1");
  chrome::AddTabAt(browser(), GURL(), -1, true);
  content::WebContents* chrome_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  PermissionBubbleManager::FromWebContents(chrome_tab)
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  InfoBarResponder infobar_responder(
      InfoBarService::FromWebContents(chrome_tab), InfoBarResponder::ACCEPT);
  ui_test_utils::NavigateToURL(browser(), room_url);

  ASSERT_TRUE(LaunchFirefoxWithUrl(room_url));

  ASSERT_TRUE(WaitForCallToComeUp(chrome_tab));

  // Ensure Firefox manages to send video our way.
  ASSERT_TRUE(DetectRemoteVideoPlaying(chrome_tab));
}
