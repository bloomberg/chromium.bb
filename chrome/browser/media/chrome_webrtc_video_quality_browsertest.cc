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
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/python_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

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
static const base::FilePath::CharType kReferenceYuvFileName[] =
    FILE_PATH_LITERAL("reference_video.yuv");
static const base::FilePath::CharType kCapturedYuvFileName[] =
    FILE_PATH_LITERAL("captured_video.yuv");
static const base::FilePath::CharType kStatsFileName[] =
    FILE_PATH_LITERAL("stats.txt");
static const char kMainWebrtcTestHtmlPage[] =
    "files/webrtc/webrtc_jsep01_test.html";
static const char kCapturingWebrtcHtmlPage[] =
    "files/webrtc/webrtc_video_quality_test.html";
static const int kVgaWidth = 640;
static const int kVgaHeight = 480;

// If you change the port number, don't forget to modify video_extraction.js
// too!
static const char kPyWebSocketPortNumber[] = "12221";

// Test the video quality of the WebRTC output.
//
// Prerequisites: This test case must run on a machine with a virtual webcam
// that plays video from the reference file located in <the running user's home
// folder>/kWorkingDirName/kReferenceYuvFileName.
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
class WebrtcVideoQualityBrowserTest : public WebRtcTestBase {
 public:
  WebrtcVideoQualityBrowserTest()
      : pywebsocket_server_(0),
        environment_(base::Environment::Create()) {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    peerconnection_server_.Start();

    // Ensure we have the stuff we need.
    EXPECT_TRUE(base::PathExists(GetWorkingDir()))
        << "Cannot find the working directory for the reference video and "
           "the temporary files:" << GetWorkingDir().value();
    base::FilePath reference_file =
        GetWorkingDir().Append(kReferenceYuvFileName);
    EXPECT_TRUE(base::PathExists(reference_file))
        << "Cannot find the reference file to be used for video quality "
        << "comparison: " << reference_file.value();
  }

  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE {
    peerconnection_server_.Stop();
    ShutdownPyWebSocketServer();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // TODO(phoglund): check that user actually has the requisite devices and
    // print a nice message if not; otherwise the test just times out which can
    // be confusing.
    // This test expects real device handling and requires a real webcam / audio
    // device; it will not work with fake devices.
    EXPECT_FALSE(
        command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream))
        << "You cannot run this test with fake devices.";

    // The video playback will not work without a GPU, so force its use here.
    command_line->AppendSwitch(switches::kUseGpuInTests);
  }

  void StartPyWebSocketServer() {
    base::FilePath path_pywebsocket_dir =
        GetSourceDir().Append(FILE_PATH_LITERAL("third_party/pywebsocket/src"));
    base::FilePath pywebsocket_server = path_pywebsocket_dir.Append(
        FILE_PATH_LITERAL("mod_pywebsocket/standalone.py"));
    base::FilePath path_to_data_handler =
        GetSourceDir().Append(FILE_PATH_LITERAL("chrome/test/functional"));

    EXPECT_TRUE(base::PathExists(pywebsocket_server))
        << "Fatal: missing pywebsocket server.";
    EXPECT_TRUE(base::PathExists(path_to_data_handler))
        << "Fatal: missing data handler for pywebsocket server.";

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

    LOG(INFO) << "Running " << pywebsocket_command.GetCommandLineString();
    EXPECT_TRUE(base::LaunchProcess(
        pywebsocket_command, base::LaunchOptions(), &pywebsocket_server_))
        << "Failed to launch pywebsocket server.";
  }

  void ShutdownPyWebSocketServer() {
    EXPECT_TRUE(base::KillProcess(pywebsocket_server_, 0, false))
        << "Failed to shut down pywebsocket server!";
  }

  // Convenience method which executes the provided javascript in the context
  // of the provided web contents and returns what it evaluated to.
  std::string ExecuteJavascript(const std::string& javascript,
                                content::WebContents* tab_contents) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        tab_contents, javascript, &result));
    return result;
  }

  // Ensures we didn't get any errors asynchronously (e.g. while no javascript
  // call from this test was outstanding).
  // TODO(phoglund): this becomes obsolete when we switch to communicating with
  // the DOM message queue.
  void AssertNoAsynchronousErrors(content::WebContents* tab_contents) {
    EXPECT_EQ("ok-no-errors",
              ExecuteJavascript("getAnyTestFailures()", tab_contents));
  }

  // The peer connection server lets our two tabs find each other and talk to
  // each other (e.g. it is the application-specific "signaling solution").
  void ConnectToPeerConnectionServer(const std::string peer_name,
                                     content::WebContents* tab_contents) {
    std::string javascript = base::StringPrintf(
        "connect('http://localhost:8888', '%s');", peer_name.c_str());
    EXPECT_EQ("ok-connected", ExecuteJavascript(javascript, tab_contents));
  }

  void EstablishCall(content::WebContents* from_tab,
                     content::WebContents* to_tab) {
    EXPECT_EQ("ok-peerconnection-created",
              ExecuteJavascript("preparePeerConnection()", from_tab));
    EXPECT_EQ("ok-added", ExecuteJavascript("addLocalStream()", from_tab));
    EXPECT_EQ("ok-negotiating", ExecuteJavascript("negotiateCall()", from_tab));

    // Ensure the call gets up on both sides.
    EXPECT_TRUE(PollingWaitUntil(
        "getPeerConnectionReadyState()", "active", from_tab));
    EXPECT_TRUE(PollingWaitUntil(
        "getPeerConnectionReadyState()", "active", to_tab));
  }

  void HangUp(content::WebContents* from_tab) {
    EXPECT_EQ("ok-call-hung-up", ExecuteJavascript("hangUp()", from_tab));
  }

  void WaitUntilHangupVerified(content::WebContents* tab_contents) {
    EXPECT_TRUE(PollingWaitUntil(
        "getPeerConnectionReadyState()", "no-peer-connection", tab_contents));
  }

  // Runs the RGBA to I420 converter on the video in |capture_video_filename|,
  // which should contain frames of size |width| x |height|.
  //
  // The rgba_to_i420_converter is part of the webrtc_test_tools target which
  // should be build prior to running this test. The resulting binary should
  // live next to Chrome.
  void RunARGBtoI420Converter(int width,
                              int height,
                              const base::FilePath& captured_video_filename) {
    base::FilePath path_to_converter = base::MakeAbsoluteFilePath(
        GetBrowserDir().Append(kArgbToI420ConverterExecutable));
    EXPECT_TRUE(base::PathExists(path_to_converter))
        << "Missing ARGB->I420 converter: should be in "
        << path_to_converter.value();

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
    LOG(INFO) << "Running " << converter_command.GetCommandLineString();
    std::string result;
    EXPECT_TRUE(base::GetAppOutput(converter_command, &result));
    LOG(INFO) << "Output was:\n\n" << result;
  }

  // Compares the |captured_video_filename| with the |reference_video_filename|.
  //
  // The barcode decoder decodes the captured video containing barcodes overlaid
  // into every frame of the video (produced by rgba_to_i420_converter). It
  // produces a set of PNG images and a |stats_file| that maps each captured
  // frame to a frame in the reference video. The frames should be of size
  // |width| x |height|. The output of compare_videos.py is returned.
  std::string CompareVideos(int width,
                            int height,
                            const base::FilePath& captured_video_filename,
                            const base::FilePath& reference_video_filename,
                            const base::FilePath& stats_file) {
    base::FilePath path_to_analyzer = base::MakeAbsoluteFilePath(
        GetBrowserDir().Append(kFrameAnalyzerExecutable));
    base::FilePath path_to_compare_script = GetSourceDir().Append(
        FILE_PATH_LITERAL("third_party/webrtc/tools/compare_videos.py"));

    EXPECT_TRUE(base::PathExists(path_to_analyzer))
        << "Missing frame analyzer: should be in " << path_to_analyzer.value();
    EXPECT_TRUE(base::PathExists(path_to_compare_script))
        << "Missing video compare script: should be in "
        << path_to_compare_script.value();

    // Note: don't append switches to this command since it will mess up the
    // -u in the python invocation!
    CommandLine compare_command(CommandLine::NO_PROGRAM);
    EXPECT_TRUE(GetPythonCommand(&compare_command));

    compare_command.AppendArgPath(path_to_compare_script);
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

    LOG(INFO) << "Running " << compare_command.GetCommandLineString();
    std::string result;
    EXPECT_TRUE(base::GetAppOutput(compare_command, &result));
    LOG(INFO) << "Output was:\n\n" << result;
    return result;
  }

  // Processes the |frame_analyzer_output| for the different frame counts.
  //
  // The frame analyzer outputs additional information about the number of
  // unique frames captured, The max number of repeated frames in a sequence and
  // the max number of skipped frames. These values are then written to the Perf
  // Graph. (Note: Some of the repeated or skipped frames will probably be due
  // to the imperfection of JavaScript timers).
  void PrintFramesCountPerfResults(std::string frame_analyzer_output) {
    size_t unique_frames_pos =
        frame_analyzer_output.rfind("Unique_frames_count");
    EXPECT_NE(unique_frames_pos, std::string::npos)
        << "Missing Unique_frames_count in frame analyzer output:\n"
        << frame_analyzer_output;

    std::string unique_frame_counts =
        frame_analyzer_output.substr(unique_frames_pos);
    // TODO(phoglund): Fix ESTATS result to not have this silly newline.
    std::replace(
        unique_frame_counts.begin(), unique_frame_counts.end(), '\n', ' ');

    std::vector<std::pair<std::string, std::string> > key_values;
    base::SplitStringIntoKeyValuePairs(
        unique_frame_counts, ':', ' ', &key_values);
    std::vector<std::pair<std::string, std::string> >::const_iterator iter;
    for (iter = key_values.begin(); iter != key_values.end(); ++iter) {
      const std::pair<std::string, std::string>& key_value = *iter;
      perf_test::PrintResult(
          key_value.first, "", "VGA", key_value.second, "", false);
    }
  }

  // Processes the |frame_analyzer_output| to extract the PSNR and SSIM values.
  //
  // The frame analyzer produces PSNR and SSIM results for every unique frame
  // that has been captured. This method forms a list of all the psnr and ssim
  // values and passes it to PrintResultList() for printing on the Perf Graph.
  void PrintPsnrAndSsimPerfResults(std::string frame_analyzer_output) {
    size_t stats_start = frame_analyzer_output.find("BSTATS");
    EXPECT_NE(stats_start, std::string::npos)
        << "Missing BSTATS in frame analyzer output:\n"
        << frame_analyzer_output;
    size_t stats_end = frame_analyzer_output.find("ESTATS");
    EXPECT_NE(stats_end, std::string::npos)
        << "Missing ESTATS in frame analyzer output:\n"
        << frame_analyzer_output;

    stats_start += std::string("BSTATS").size();
    std::string psnr_ssim_stats =
        frame_analyzer_output.substr(stats_start, stats_end - stats_start);

    // PSNR and SSIM values aren't really key-value pairs but it is convenient
    // to parse them as such.
    // TODO(phoglund): make the format more convenient so we need less
    // processing here.
    std::vector<std::pair<std::string, std::string> > psnr_ssim_entries;
    base::SplitStringIntoKeyValuePairs(
        psnr_ssim_stats, ' ', ';', &psnr_ssim_entries);

    std::string psnr_value_list;
    std::string ssim_value_list;
    std::vector<std::pair<std::string, std::string> >::const_iterator iter;
    for (iter = psnr_ssim_entries.begin(); iter != psnr_ssim_entries.end();
         ++iter) {
      const std::pair<std::string, std::string>& psnr_and_ssim = *iter;
      psnr_value_list.append(psnr_and_ssim.first).append(",");
      ssim_value_list.append(psnr_and_ssim.second).append(",");
    }
    // Nuke last comma.
    psnr_value_list.erase(psnr_value_list.size() - 1);
    ssim_value_list.erase(ssim_value_list.size() - 1);

    perf_test::PrintResultList("PSNR", "", "VGA", psnr_value_list, "dB", false);
    perf_test::PrintResultList("SSIM", "", "VGA", ssim_value_list, "", false);
  }

  base::FilePath GetWorkingDir() {
    std::string home_dir;
    environment_->GetVar(kHomeEnvName, &home_dir);
    base::FilePath::StringType native_home_dir(home_dir.begin(),
                                               home_dir.end());
    return base::FilePath(native_home_dir).Append(kWorkingDirName);
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

  PeerConnectionServerRunner peerconnection_server_;
  base::ProcessHandle pywebsocket_server_;
  scoped_ptr<base::Environment> environment_;
};

IN_PROC_BROWSER_TEST_F(WebrtcVideoQualityBrowserTest,
                       MANUAL_TestVGAVideoQuality) {
  StartPyWebSocketServer();

  EXPECT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kMainWebrtcTestHtmlPage));
  content::WebContents* left_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GetUserMediaAndAccept(left_tab);

  chrome::AddBlankTabAt(browser(), -1, true);
  content::WebContents* right_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(browser(),
                               test_server()->GetURL(kCapturingWebrtcHtmlPage));
  GetUserMediaAndAccept(right_tab);

  ConnectToPeerConnectionServer("peer 1", left_tab);
  ConnectToPeerConnectionServer("peer 2", right_tab);

  EstablishCall(left_tab, right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  // Poll slower here to avoid flooding the log with messages: capturing and
  // sending frames take quite a bit of time.
  int polling_interval_msec = 1000;

  EXPECT_TRUE(PollingWaitUntil(
      "doneFrameCapturing()", "done-capturing", right_tab,
      polling_interval_msec));

  HangUp(left_tab);
  WaitUntilHangupVerified(left_tab);
  WaitUntilHangupVerified(right_tab);

  AssertNoAsynchronousErrors(left_tab);
  AssertNoAsynchronousErrors(right_tab);

  EXPECT_TRUE(PollingWaitUntil(
      "haveMoreFramesToSend()", "no-more-frames", right_tab,
      polling_interval_msec));

  RunARGBtoI420Converter(
      kVgaWidth, kVgaHeight, GetWorkingDir().Append(kCapturedYuvFileName));
  std::string output =
      CompareVideos(kVgaWidth,
                    kVgaHeight,
                    GetWorkingDir().Append(kCapturedYuvFileName),
                    GetWorkingDir().Append(kReferenceYuvFileName),
                    GetWorkingDir().Append(kStatsFileName));

  PrintFramesCountPerfResults(output);
  PrintPsnrAndSsimPerfResults(output);
}
