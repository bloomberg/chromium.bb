// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/python_utils.h"
#include "testing/perf/perf_test.h"
#include "ui/gl/gl_switches.h"

// For fine-grained suppression on flaky tests.
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

static const base::FilePath::CharType kFrameAnalyzerExecutable[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("frame_analyzer.exe");
#else
    FILE_PATH_LITERAL("frame_analyzer");
#endif

static const base::FilePath::CharType kArgbToI420ConverterExecutable[] =
#if defined(OS_WIN)
    FILE_PATH_LITERAL("rgba_to_i420_converter.exe");
#else
    FILE_PATH_LITERAL("rgba_to_i420_converter");
#endif

static const char kHomeEnvName[] =
#if defined(OS_WIN)
    "HOMEPATH";
#else
    "HOME";
#endif

// The working dir should be in the user's home folder.
static const base::FilePath::CharType kWorkingDirName[] =
    FILE_PATH_LITERAL("webrtc_video_quality");
static const base::FilePath::CharType kCapturedYuvFileName[] =
    FILE_PATH_LITERAL("captured_video.yuv");
static const base::FilePath::CharType kStatsFileName[] =
    FILE_PATH_LITERAL("stats.txt");
static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";
static const char kCapturingWebrtcHtmlPage[] =
    "/webrtc/webrtc_video_quality_test.html";
static const int k360pWidth = 640;
static const int k360pHeight = 360;

// If you change the port number, don't forget to modify video_extraction.js
// too!
static const char kPyWebSocketPortNumber[] = "12221";

// Test the video quality of the WebRTC output.
//
// Prerequisites: This test case must run on a machine with a chrome playing
// the video from the reference files located int GetReferenceVideosDir().
// The file kReferenceY4mFileName.kY4mFileExtension is played using a
// FileVideoCaptureDevice and its sibling with kYuvFileExtension is used for
// comparison.
//
// You must also compile the chromium_builder_webrtc target before you run this
// test to get all the tools built.
//
// The external compare_videos.py script also depends on two external
// executables which must be located in the PATH when running this test.
// * zxing (see the CPP version at https://code.google.com/p/zxing)
// * ffmpeg 0.11.1 or compatible version (see http://www.ffmpeg.org)
//
// The test case will launch a custom binary (peerconnection_server) which will
// allow two WebRTC clients to find each other.
//
// The test also runs several other custom binaries - rgba_to_i420 converter and
// frame_analyzer. Both tools can be found under third_party/webrtc/tools. The
// test also runs a stand alone Python implementation of a WebSocket server
// (pywebsocket) and a barcode_decoder script.
class WebRtcVideoQualityBrowserTest : public WebRtcTestBase {
 public:
  WebRtcVideoQualityBrowserTest()
      : pywebsocket_server_(0),
        environment_(base::Environment::Create()) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    test::PeerConnectionServerRunner::KillAllPeerConnectionServers();
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Set up the command line option with the expected file name. We will check
    // its existence in HasAllRequiredResources().
    webrtc_reference_video_y4m_ = test::GetReferenceVideosDir()
        .Append(test::kReferenceFileName360p)
        .AddExtension(test::kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   webrtc_reference_video_y4m_);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  bool HasAllRequiredResources() {
    if (!base::PathExists(GetWorkingDir())) {
      LOG(ERROR) << "Cannot find the working directory for the temporary "
          "files:" << GetWorkingDir().value();
      return false;
    }

    // Ensure we have the required input files.
    return test::HasReferenceFilesInCheckout();
  }

  bool StartPyWebSocketServer() {
    base::FilePath path_pywebsocket_dir =
        GetSourceDir().Append(FILE_PATH_LITERAL("third_party/pywebsocket/src"));
    base::FilePath pywebsocket_server = path_pywebsocket_dir.Append(
        FILE_PATH_LITERAL("mod_pywebsocket/standalone.py"));
    base::FilePath path_to_data_handler =
        GetSourceDir().Append(FILE_PATH_LITERAL("chrome/test/functional"));

    if (!base::PathExists(pywebsocket_server)) {
      LOG(ERROR) << "Missing pywebsocket server.";
      return false;
    }
    if (!base::PathExists(path_to_data_handler)) {
      LOG(ERROR) << "Missing data handler for pywebsocket server.";
      return false;
    }

    AppendToPythonPath(path_pywebsocket_dir);

    // Note: don't append switches to this command since it will mess up the
    // -u in the python invocation!
    CommandLine pywebsocket_command(CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&pywebsocket_command));

    pywebsocket_command.AppendArgPath(pywebsocket_server);
    pywebsocket_command.AppendArg("-p");
    pywebsocket_command.AppendArg(kPyWebSocketPortNumber);
    pywebsocket_command.AppendArg("-d");
    pywebsocket_command.AppendArgPath(path_to_data_handler);

    VLOG(0) << "Running " << pywebsocket_command.GetCommandLineString();
    return base::LaunchProcess(pywebsocket_command, base::LaunchOptions(),
                               &pywebsocket_server_);
  }

  bool ShutdownPyWebSocketServer() {
    return base::KillProcess(pywebsocket_server_, 0, false);
  }

  // Runs the RGBA to I420 converter on the video in |capture_video_filename|,
  // which should contain frames of size |width| x |height|.
  //
  // The rgba_to_i420_converter is part of the webrtc_test_tools target which
  // should be build prior to running this test. The resulting binary should
  // live next to Chrome.
  bool RunARGBtoI420Converter(int width,
                              int height,
                              const base::FilePath& captured_video_filename) {
    base::FilePath path_to_converter = base::MakeAbsoluteFilePath(
        GetBrowserDir().Append(kArgbToI420ConverterExecutable));

    if (!base::PathExists(path_to_converter)) {
      LOG(ERROR) << "Missing ARGB->I420 converter: should be in "
          << path_to_converter.value();
      return false;
    }

    CommandLine converter_command(path_to_converter);
    converter_command.AppendSwitchPath("--frames_dir", GetWorkingDir());
    converter_command.AppendSwitchPath("--output_file",
                                       captured_video_filename);
    converter_command.AppendSwitchASCII("--width",
                                        base::StringPrintf("%d", width));
    converter_command.AppendSwitchASCII("--height",
                                        base::StringPrintf("%d", height));

    // We produce an output file that will later be used as an input to the
    // barcode decoder and frame analyzer tools.
    VLOG(0) << "Running " << converter_command.GetCommandLineString();
    std::string result;
    bool ok = base::GetAppOutput(converter_command, &result);
    VLOG(0) << "Output was:\n\n" << result;
    return ok;
  }

  // Compares the |captured_video_filename| with the |reference_video_filename|.
  //
  // The barcode decoder decodes the captured video containing barcodes overlaid
  // into every frame of the video (produced by rgba_to_i420_converter). It
  // produces a set of PNG images and a |stats_file| that maps each captured
  // frame to a frame in the reference video. The frames should be of size
  // |width| x |height|.
  // All measurements calculated are printed as perf parsable numbers to stdout.
  bool CompareVideosAndPrintResult(
      int width,
      int height,
      const base::FilePath& captured_video_filename,
      const base::FilePath& reference_video_filename,
      const base::FilePath& stats_file) {

    base::FilePath path_to_analyzer = base::MakeAbsoluteFilePath(
        GetBrowserDir().Append(kFrameAnalyzerExecutable));
    base::FilePath path_to_compare_script = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/tools/compare_videos.py"));

    if (!base::PathExists(path_to_analyzer)) {
      LOG(ERROR) << "Missing frame analyzer: should be in "
          << path_to_analyzer.value();
      return false;
    }
    if (!base::PathExists(path_to_compare_script)) {
      LOG(ERROR) << "Missing video compare script: should be in "
          << path_to_compare_script.value();
      return false;
    }

    // Note: don't append switches to this command since it will mess up the
    // -u in the python invocation!
    CommandLine compare_command(CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&compare_command));

    compare_command.AppendArgPath(path_to_compare_script);
    compare_command.AppendArg("--label=360p");
    compare_command.AppendArg("--ref_video");
    compare_command.AppendArgPath(reference_video_filename);
    compare_command.AppendArg("--test_video");
    compare_command.AppendArgPath(captured_video_filename);
    compare_command.AppendArg("--frame_analyzer");
    compare_command.AppendArgPath(path_to_analyzer);
    compare_command.AppendArg("--yuv_frame_width");
    compare_command.AppendArg(base::StringPrintf("%d", width));
    compare_command.AppendArg("--yuv_frame_height");
    compare_command.AppendArg(base::StringPrintf("%d", height));
    compare_command.AppendArg("--stats_file");
    compare_command.AppendArgPath(stats_file);

    VLOG(0) << "Running " << compare_command.GetCommandLineString();
    std::string output;
    bool ok = base::GetAppOutput(compare_command, &output);
    // Print to stdout to ensure the perf numbers are parsed properly by the
    // buildbot step.
    printf("Output was:\n\n%s\n", output.c_str());
    return ok;
  }

  base::FilePath GetWorkingDir() {
    std::string home_dir;
    environment_->GetVar(kHomeEnvName, &home_dir);
    base::FilePath::StringType native_home_dir(home_dir.begin(),
                                               home_dir.end());
    return base::FilePath(native_home_dir).Append(kWorkingDirName);
  }

  test::PeerConnectionServerRunner peerconnection_server_;

 private:
  base::FilePath GetSourceDir() {
    base::FilePath source_dir;
    PathService::Get(base::DIR_SOURCE_ROOT, &source_dir);
    return source_dir;
  }

  base::FilePath GetBrowserDir() {
    base::FilePath browser_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &browser_dir));
    return browser_dir;
  }

  base::ProcessHandle pywebsocket_server_;
  scoped_ptr<base::Environment> environment_;
  base::FilePath webrtc_reference_video_y4m_;
};

// Disabled due to crbug.com/360033.
IN_PROC_BROWSER_TEST_F(WebRtcVideoQualityBrowserTest,
                       DISABLED_MANUAL_TestVGAVideoQuality) {
#if defined(OS_WIN)
  // Fails on XP. http://crbug.com/353078
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 150) <<
      "This is a long-running test; you must specify "
      "--ui-test-action-max-timeout to have a value of at least 150000.";

  ASSERT_TRUE(HasAllRequiredResources());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ASSERT_TRUE(StartPyWebSocketServer());
  ASSERT_TRUE(peerconnection_server_.Start());

  content::WebContents* left_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(
          embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
          kAudioVideoCallConstraints360p);
  content::WebContents* right_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(
          embedded_test_server()->GetURL(kCapturingWebrtcHtmlPage),
          kAudioVideoCallConstraints360p);

  EstablishCall(left_tab, right_tab);

  // Poll slower here to avoid flooding the log with messages: capturing and
  // sending frames take quite a bit of time.
  int polling_interval_msec = 1000;

  EXPECT_TRUE(test::PollingWaitUntil(
      "doneFrameCapturing()", "done-capturing", right_tab,
      polling_interval_msec));

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  EXPECT_TRUE(test::PollingWaitUntil(
      "haveMoreFramesToSend()", "no-more-frames", right_tab,
      polling_interval_msec));

  // Shut everything down to avoid having the javascript race with the analysis
  // tools. For instance, dont have console log printouts interleave with the
  // RESULT lines from the analysis tools (crbug.com/323200).
  ASSERT_TRUE(peerconnection_server_.Stop());
  ASSERT_TRUE(ShutdownPyWebSocketServer());

  chrome::CloseWebContents(browser(), left_tab, false);
  chrome::CloseWebContents(browser(), right_tab, false);

  RunARGBtoI420Converter(
      k360pWidth, k360pHeight, GetWorkingDir().Append(kCapturedYuvFileName));
  ASSERT_TRUE(CompareVideosAndPrintResult(
      k360pWidth,
      k360pHeight,
      GetWorkingDir().Append(kCapturedYuvFileName),
      test::GetReferenceVideosDir()
          .Append(test::kReferenceFileName360p)
          .AddExtension(test::kYuvFileExtension),
      GetWorkingDir().Append(kStatsFileName)));
}
