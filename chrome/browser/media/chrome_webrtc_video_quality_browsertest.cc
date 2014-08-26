// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
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
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/python_utils.h"
#include "testing/perf/perf_test.h"
#include "ui/gl/gl_switches.h"

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

static const base::FilePath::CharType kCapturedYuvFileName[] =
    FILE_PATH_LITERAL("captured_video.yuv");
static const base::FilePath::CharType kStatsFileName[] =
    FILE_PATH_LITERAL("stats.txt");
static const char kMainWebrtcTestHtmlPage[] =
    "/webrtc/webrtc_jsep01_test.html";
static const char kCapturingWebrtcHtmlPage[] =
    "/webrtc/webrtc_video_quality_test.html";

static const struct VideoQualityTestConfig {
  const char* test_name;
  int width;
  int height;
  const base::FilePath::CharType* reference_video;
  const char* constraints;
} kVideoConfigurations[] = {
  { "360p", 640, 360,
    test::kReferenceFileName360p,
    WebRtcTestBase::kAudioVideoCallConstraints360p },
  { "720p", 1280, 720,
    test::kReferenceFileName720p,
    WebRtcTestBase::kAudioVideoCallConstraints720p },
};

// Test the video quality of the WebRTC output.
//
// Prerequisites: This test case must run on a machine with a chrome playing
// the video from the reference files located in GetReferenceFilesDir().
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
// The test runs several custom binaries - rgba_to_i420 converter and
// frame_analyzer. Both tools can be found under third_party/webrtc/tools. The
// test also runs a stand alone Python implementation of a WebSocket server
// (pywebsocket) and a barcode_decoder script.
class WebRtcVideoQualityBrowserTest : public WebRtcTestBase,
    public testing::WithParamInterface<VideoQualityTestConfig> {
 public:
  WebRtcVideoQualityBrowserTest()
      : environment_(base::Environment::Create()) {
    test_config_ = GetParam();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.

    ASSERT_TRUE(temp_working_dir_.CreateUniqueTempDir());
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Set up the command line option with the expected file name. We will check
    // its existence in HasAllRequiredResources().
    webrtc_reference_video_y4m_ = test::GetReferenceFilesDir()
        .Append(test_config_.reference_video)
        .AddExtension(test::kY4mFileExtension);
    command_line->AppendSwitchPath(switches::kUseFileForFakeVideoCapture,
                                   webrtc_reference_video_y4m_);
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  // Writes all frames we've captured so far by grabbing them from the
  // javascript and writing them to the temporary work directory.
  void WriteCapturedFramesToWorkingDir(content::WebContents* capturing_tab) {
    int num_frames = 0;
    std::string response =
        ExecuteJavascript("getTotalNumberCapturedFrames()", capturing_tab);
    ASSERT_TRUE(base::StringToInt(response, &num_frames)) <<
        "Failed to retrieve frame count: got " << response;
    ASSERT_NE(0, num_frames) << "Failed to capture any frames.";

    for (int i = 0; i < num_frames; i++) {
      std::string base64_encoded_frame =
          ExecuteJavascript(base::StringPrintf("getOneCapturedFrame(%d)", i),
                            capturing_tab);
      std::string decoded_frame;
      ASSERT_TRUE(base::Base64Decode(base64_encoded_frame, &decoded_frame))
          << "Failed to decode frame data '" << base64_encoded_frame << "'.";

      std::string file_name = base::StringPrintf("frame_%04d", i);
      base::File frame_file(GetWorkingDir().AppendASCII(file_name),
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      size_t written = frame_file.Write(0, decoded_frame.c_str(),
                                        decoded_frame.length());
      ASSERT_EQ(decoded_frame.length(), written);
    }
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
    converter_command.AppendSwitchASCII("--delete_frames", "true");

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
      const char* test_label,
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
    compare_command.AppendArg(base::StringPrintf("--label=%s", test_label));
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

 protected:
  VideoQualityTestConfig test_config_;

  base::FilePath GetWorkingDir() {
    return temp_working_dir_.path();
  }

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

  scoped_ptr<base::Environment> environment_;
  base::FilePath webrtc_reference_video_y4m_;
  base::ScopedTempDir temp_working_dir_;
};

INSTANTIATE_TEST_CASE_P(
    WebRtcVideoQualityBrowserTests,
    WebRtcVideoQualityBrowserTest,
    testing::ValuesIn(kVideoConfigurations));

IN_PROC_BROWSER_TEST_P(WebRtcVideoQualityBrowserTest,
                       MANUAL_TestVideoQuality) {
  if (OnWinXp())
    return;  // Fails on XP. http://crbug.com/353078.

  ASSERT_GE(TestTimeouts::action_max_timeout().InSeconds(), 150) <<
      "This is a long-running test; you must specify "
      "--ui-test-action-max-timeout to have a value of at least 150000.";
  ASSERT_TRUE(test::HasReferenceFilesInCheckout());
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  content::WebContents* left_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(
          embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage),
          test_config_.constraints);
  content::WebContents* right_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(
          embedded_test_server()->GetURL(kCapturingWebrtcHtmlPage),
          test_config_.constraints);

  SetupPeerconnectionWithLocalStream(left_tab);
  SetupPeerconnectionWithLocalStream(right_tab);

  NegotiateCall(left_tab, right_tab);

  // Poll slower here to avoid flooding the log with messages: capturing and
  // sending frames take quite a bit of time.
  int polling_interval_msec = 1000;

  EXPECT_TRUE(test::PollingWaitUntil(
      "doneFrameCapturing()", "done-capturing", right_tab,
      polling_interval_msec));

  HangUp(left_tab);

  WriteCapturedFramesToWorkingDir(right_tab);

  // Shut everything down to avoid having the javascript race with the analysis
  // tools. For instance, dont have console log printouts interleave with the
  // RESULT lines from the analysis tools (crbug.com/323200).
  chrome::CloseWebContents(browser(), left_tab, false);
  chrome::CloseWebContents(browser(), right_tab, false);

  ASSERT_TRUE(RunARGBtoI420Converter(
      test_config_.width, test_config_.height,
      GetWorkingDir().Append(kCapturedYuvFileName)));
  ASSERT_TRUE(CompareVideosAndPrintResult(
      test_config_.test_name,
      test_config_.width,
      test_config_.height,
      GetWorkingDir().Append(kCapturedYuvFileName),
      test::GetReferenceFilesDir()
          .Append(test_config_.reference_video)
          .AddExtension(test::kYuvFileExtension),
      GetWorkingDir().Append(kStatsFileName)));
}
