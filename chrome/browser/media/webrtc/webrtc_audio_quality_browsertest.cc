// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <ctime>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_audio.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"

namespace {

static const base::FilePath::CharType kReferenceFile[] =
    FILE_PATH_LITERAL("speech_44kHz_16bit_stereo.wav");

// The javascript will load the reference file relative to its location,
// which is in /webrtc on the web server. The files we are looking for are in
// webrtc/resources in the chrome/test/data folder.
static const char kReferenceFileRelativeUrl[] =
    "resources/speech_44kHz_16bit_stereo.wav";

static const char kWebRtcAudioTestHtmlPage[] =
    "/webrtc/webrtc_audio_quality_test.html";

// How long to record the audio in the receiving peerConnection.
static const int kCaptureDurationInSeconds = 25;

// The name where the recorded WebM audio file will be saved.
static const char kWebmRecordingFilename[] = "recording.webm";

// How often to ask the test page whether the audio recording is completed.
const int kPollingIntervalInMs = 1000;

// For the AGC test, there are 6 speech segments split on silence. If one
// segment is significantly different in length compared to the same segment in
// the reference file, there's something fishy going on.
const int kMaxAgcSegmentDiffMs =
#if defined(OS_MACOSX)
  // Something is different on Mac; http://crbug.com/477653.
  600;
#else
  200;
#endif

#if defined(OS_LINUX) || defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_WebRtcAudioQualityBrowserTest WebRtcAudioQualityBrowserTest
#else
// Not implemented on Android, ChromeOS etc.
#define MAYBE_WebRtcAudioQualityBrowserTest DISABLED_WebRtcAudioQualityBrowserTest
#endif

}  // namespace

// Test we can set up a WebRTC call and play audio through it.
//
// If you're not a googler and want to run this test, you need to provide a
// pesq binary for your platform (and sox.exe on windows). Read more on how
// resources are managed in chrome/test/data/webrtc/resources/README.
//
// On Linux:
// 1. # sudo apt-get install sox
//
// On Mac:
// TODO(phoglund): download sox from gs instead.
// 1. Get SoundFlower: http://rogueamoeba.com/freebies/soundflower/download.php
// 2. Install it + reboot.
// 3. Install MacPorts (http://www.macports.org/).
// 4. Install sox: sudo port install sox.
// 5. (For Chrome bots) Ensure sox is reachable from the env the test
//    executes in (sox tends to install in /opt/, which generally isn't in the
//    Chrome bots' env). For instance, run
//    sudo ln -s /opt/local/bin/sox /usr/local/bin/sox
class MAYBE_WebRtcAudioQualityBrowserTest : public WebRtcTestBase {
 public:
  MAYBE_WebRtcAudioQualityBrowserTest() {}
  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpOnMainThread() override {
    base::FilePath tmp_dir;
    EXPECT_TRUE(base::GetTempDir(&tmp_dir));
    webm_recorded_output_filename_ =
        tmp_dir.Append(FILE_PATH_LITERAL("recording.webm"));

    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, tmp_dir);
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    EXPECT_FALSE(command_line->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    // The WebAudio-based tests don't care what devices are available to
    // getUserMedia, and the getUserMedia-based tests will play back a file
    // through the fake device using using --use-file-for-fake-audio-capture.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Add loopback interface such that there is always connectivity.
    command_line->AppendSwitch(switches::kAllowLoopbackInPeerConnection);
  }

  void ConfigureFakeDeviceToPlayFile(const base::FilePath& wav_file_path) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
        switches::kUseFileForFakeAudioCapture,
        wav_file_path.value() + FILE_PATH_LITERAL("%noloop"));
  }

  void AddAudioFileToWebAudio(const std::string& input_file_relative_url,
                              content::WebContents* tab_contents) {
    // This calls into webaudio.js.
    EXPECT_EQ("ok-added", ExecuteJavascript(
        "addAudioFile('" + input_file_relative_url + "')", tab_contents));
  }

  void PlayAudioFileThroughWebAudio(content::WebContents* tab_contents) {
    EXPECT_EQ("ok-playing", ExecuteJavascript("playAudioFile()", tab_contents));
  }

  content::WebContents* OpenPageWithoutGetUserMedia(const char* url) {
    chrome::AddTabAt(browser(), GURL(), -1, true);
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(url));
    content::WebContents* tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Prepare the peer connections manually in this test since we don't add
    // getUserMedia-derived media streams in this test like the other tests.
    EXPECT_EQ("ok-peerconnection-created",
              ExecuteJavascript("preparePeerConnection()", tab));
    return tab;
  }

  void MuteMediaElement(const std::string& element_id,
                        content::WebContents* tab_contents) {
    EXPECT_EQ("ok-muted", ExecuteJavascript(
        "setMediaElementMuted('" + element_id + "', true)", tab_contents));
  }

 protected:
  void TestAutoGainControl(const base::FilePath::StringType& reference_filename,
                           const std::string& constraints,
                           const std::string& perf_modifier);
  void SetupAndRecordAudioCall(const base::FilePath& reference_file,
                               const base::FilePath& recorded_output_path,
                               const std::string& constraints);
  void TestWithFakeDeviceGetUserMedia(const std::string& constraints,
                                      const std::string& perf_modifier);

  base::FilePath webm_recorded_output_filename_;
};

namespace {

// Sox is the "Swiss army knife" of audio processing. We mainly use it for
// silence trimming. See http://sox.sourceforge.net.
base::CommandLine MakeSoxCommandLine() {
#if defined(OS_WIN)
  base::FilePath sox_path = test::GetToolForPlatform("sox");
  if (!base::PathExists(sox_path)) {
    LOG(ERROR) << "Missing sox.exe binary in " << sox_path.value()
               << "; you may have to provide this binary yourself.";
    return base::CommandLine(base::CommandLine::NO_PROGRAM);
  }
  base::CommandLine command_line(sox_path);
#else
  // TODO(phoglund): call checked-in sox rather than system sox on mac/linux.
  // Same for rec invocations on Mac, above.
  base::CommandLine command_line(base::FilePath(FILE_PATH_LITERAL("sox")));
#endif
  return command_line;
}

// Removes silence from beginning and end of the |input_audio_file| and writes
// the result to the |output_audio_file|. Returns true on success.
bool RemoveSilence(const base::FilePath& input_file,
                   const base::FilePath& output_file) {
  // SOX documentation for silence command: http://sox.sourceforge.net/sox.html
  // To remove the silence from both beginning and end of the audio file, we
  // call sox silence command twice: once on normal file and again on its
  // reverse, then we reverse the final output.
  // Silence parameters are (in sequence):
  // ABOVE_PERIODS: The period for which silence occurs. Value 1 is used for
  //                 silence at beginning of audio.
  // DURATION: the amount of time in seconds that non-silence must be detected
  //           before sox stops trimming audio.
  // THRESHOLD: value used to indicate what sample value is treats as silence.
  const char* kAbovePeriods = "1";
  const char* kDuration = "2";
  const char* kTreshold = "1.5%";

  base::CommandLine command_line = MakeSoxCommandLine();
  if (command_line.GetProgram().empty())
    return false;
  command_line.AppendArgPath(input_file);
  command_line.AppendArgPath(output_file);
  command_line.AppendArg("silence");
  command_line.AppendArg(kAbovePeriods);
  command_line.AppendArg(kDuration);
  command_line.AppendArg(kTreshold);
  command_line.AppendArg("reverse");
  command_line.AppendArg("silence");
  command_line.AppendArg(kAbovePeriods);
  command_line.AppendArg(kDuration);
  command_line.AppendArg(kTreshold);
  command_line.AppendArg("reverse");

  DVLOG(0) << "Running " << command_line.GetCommandLineString();
  std::string result;
  bool ok = base::GetAppOutput(command_line, &result);
  DVLOG(0) << "Output was:\n\n" << result;
  return ok;
}

// Runs ffmpeg on the captured webm video and writes it to a .wav file.
bool RunWebmToWavConverter(const base::FilePath& webm_recorded_output_path,
                           const base::FilePath& wav_recorded_output_path) {
  const base::FilePath path_to_ffmpeg = test::GetToolForPlatform("ffmpeg");
  if (!base::PathExists(path_to_ffmpeg)) {
    LOG(ERROR) << "Missing ffmpeg: should be in " << path_to_ffmpeg.value();
    return false;
  }

  // Set up ffmpeg to output at a certain bitrate (-ab). This is hopefully set
  // high enough to avoid degrading audio quality too much.
  base::CommandLine ffmpeg_command(path_to_ffmpeg);
  ffmpeg_command.AppendArg("-i");
  ffmpeg_command.AppendArgPath(webm_recorded_output_path);
  ffmpeg_command.AppendArg("-ab");
  ffmpeg_command.AppendArg("300k");
  ffmpeg_command.AppendArg("-y");
  ffmpeg_command.AppendArgPath(wav_recorded_output_path);

  // We produce an output file that will later be used as an input to the
  // barcode decoder and frame analyzer tools.
  DVLOG(0) << "Running " << ffmpeg_command.GetCommandLineString();
  std::string result;
  bool ok = base::GetAppOutputAndError(ffmpeg_command, &result);
  DVLOG(0) << "Output was:\n\n" << result;
  return ok;
}

// Looks for 0.2 second audio segments surrounded by silences under 0.3% audio
// power and splits the input file on those silences. Output files are written
// according to the output file template (e.g. /tmp/out.wav writes
// /tmp/out001.wav, /tmp/out002.wav, etc if there are two silence-padded
// regions in the file). The silences between speech segments must be at
// least 500 ms for this to be reliable.
bool SplitFileOnSilence(const base::FilePath& input_file,
                        const base::FilePath& output_file_template) {
  base::CommandLine command_line = MakeSoxCommandLine();
  if (command_line.GetProgram().empty())
    return false;

  // These are experimentally determined and work on the files we use.
  const char* kAbovePeriods = "1";
  const char* kUnderPeriods = "1";
  const char* kDuration = "0.2";
  const char* kTreshold = "0.5%";
  command_line.AppendArgPath(input_file);
  command_line.AppendArgPath(output_file_template);
  command_line.AppendArg("silence");
  command_line.AppendArg(kAbovePeriods);
  command_line.AppendArg(kDuration);
  command_line.AppendArg(kTreshold);
  command_line.AppendArg(kUnderPeriods);
  command_line.AppendArg(kDuration);
  command_line.AppendArg(kTreshold);
  command_line.AppendArg(":");
  command_line.AppendArg("newfile");
  command_line.AppendArg(":");
  command_line.AppendArg("restart");

  DVLOG(0) << "Running " << command_line.GetCommandLineString();
  std::string result;
  bool ok = base::GetAppOutput(command_line, &result);
  DVLOG(0) << "Output was:\n\n" << result;
  return ok;
}

bool CanParseAsFloat(const std::string& value) {
  return atof(value.c_str()) != 0 || value == "0";
}

// Runs PESQ to compare |reference_file| to a |actual_file|. The |sample_rate|
// can be either 16000 or 8000.
//
// PESQ is only mono-aware, so the files should preferably be recorded in mono.
// Furthermore it expects the file to be 16 rather than 32 bits, even though
// 32 bits might work. The audio bandwidth of the two files should be the same
// e.g. don't compare a 32 kHz file to a 8 kHz file.
//
// The raw score in MOS is written to |raw_mos|, whereas the MOS-LQO score is
// written to mos_lqo. The scores are returned as floats in string form (e.g.
// "3.145", etc). Returns true on success.
bool RunPesq(const base::FilePath& reference_file,
             const base::FilePath& actual_file,
             int sample_rate, std::string* raw_mos, std::string* mos_lqo) {
  // PESQ will break if the paths are too long (!).
  EXPECT_LT(reference_file.value().length(), 128u);
  EXPECT_LT(actual_file.value().length(), 128u);

  base::FilePath pesq_path = test::GetToolForPlatform("pesq");
  if (!base::PathExists(pesq_path)) {
    LOG(ERROR) << "Missing PESQ binary in " << pesq_path.value()
               << "; you may have to provide this binary yourself.";
    return false;
  }

  base::CommandLine command_line(pesq_path);
  command_line.AppendArg(base::StringPrintf("+%d", sample_rate));
  command_line.AppendArgPath(reference_file);
  command_line.AppendArgPath(actual_file);

  DVLOG(0) << "Running " << command_line.GetCommandLineString();
  std::string result;
  if (!base::GetAppOutput(command_line, &result)) {
    LOG(ERROR) << "Failed to run PESQ.";
    return false;
  }
  DVLOG(0) << "Output was:\n\n" << result;

  const std::string result_anchor = "Prediction (Raw MOS, MOS-LQO):  = ";
  std::size_t anchor_pos = result.find(result_anchor);
  if (anchor_pos == std::string::npos) {
    LOG(ERROR) << "PESQ was not able to compute a score; we probably recorded "
        << "only silence. Please check the output/input volume levels.";
    return false;
  }

  // There are two tab-separated numbers on the format x.xxx, e.g. 5 chars each.
  std::size_t first_number_pos = anchor_pos + result_anchor.length();
  *raw_mos = result.substr(first_number_pos, 5);
  EXPECT_TRUE(CanParseAsFloat(*raw_mos)) << "Failed to parse raw MOS number.";
  *mos_lqo = result.substr(first_number_pos + 5 + 1, 5);
  EXPECT_TRUE(CanParseAsFloat(*mos_lqo)) << "Failed to parse MOS LQO number.";

  return true;
}

base::FilePath CreateTemporaryWaveFile() {
  base::FilePath filename;
  EXPECT_TRUE(base::CreateTemporaryFile(&filename));
  base::FilePath wav_filename =
      filename.AddExtension(FILE_PATH_LITERAL(".wav"));
  EXPECT_TRUE(base::Move(filename, wav_filename));
  return wav_filename;
}

void DeleteFileUnlessTestFailed(const base::FilePath& path, bool recursive) {
  if (::testing::Test::HasFailure())
    printf("Test failed; keeping recording(s) at\n\t%" PRFilePath ".\n",
           path.value().c_str());
  else
    EXPECT_TRUE(base::DeleteFile(path, recursive));
}

std::vector<base::FilePath> ListWavFilesInDir(const base::FilePath& dir) {
  base::FileEnumerator files(dir, false, base::FileEnumerator::FILES,
                             FILE_PATH_LITERAL("*.wav"));

  std::vector<base::FilePath> result;
  for (base::FilePath name = files.Next(); !name.empty(); name = files.Next())
    result.push_back(name);
  return result;
}

// Splits |to_split| into sub-files based on silence. The file you use must have
// at least 500 ms periods of silence between speech segments for this to be
// reliable.
void SplitFileOnSilenceIntoDir(const base::FilePath& to_split,
                               const base::FilePath& workdir) {
  // First trim beginning and end since they are tricky for the splitter.
  base::FilePath trimmed_audio = CreateTemporaryWaveFile();

  ASSERT_TRUE(RemoveSilence(to_split, trimmed_audio));
  DVLOG(0) << "Trimmed silence: " << trimmed_audio.value() << std::endl;

  ASSERT_TRUE(SplitFileOnSilence(
      trimmed_audio, workdir.Append(FILE_PATH_LITERAL("output.wav"))));
  DeleteFileUnlessTestFailed(trimmed_audio, false);
}

// Computes the difference between the actual and reference segment. A positive
// number x means the actual file is x dB stronger than the reference.
float AnalyzeOneSegment(const base::FilePath& ref_segment,
                        const base::FilePath& actual_segment,
                        int segment_number) {
  media::AudioParameters ref_parameters;
  media::AudioParameters actual_parameters;
  float ref_energy =
      test::ComputeAudioEnergyForWavFile(ref_segment, &ref_parameters);
  float actual_energy =
      test::ComputeAudioEnergyForWavFile(actual_segment, &actual_parameters);

  base::TimeDelta difference_in_length = ref_parameters.GetBufferDuration() -
                                         actual_parameters.GetBufferDuration();

  EXPECT_LE(difference_in_length,
            base::TimeDelta::FromMilliseconds(kMaxAgcSegmentDiffMs))
      << "Segments differ " << difference_in_length.InMilliseconds() << " ms "
      << "in length for segment " << segment_number << "; we're likely "
      << "comparing unrelated segments or silence splitting is busted.";

  return actual_energy - ref_energy;
}

std::string MakeTraceName(const base::FilePath& ref_filename,
                          size_t segment_number) {
  std::string ascii_filename;
#if defined(OS_WIN)
  ascii_filename = base::WideToUTF8(ref_filename.BaseName().value());
#else
  ascii_filename = ref_filename.BaseName().value();
#endif
  return base::StringPrintf(
      "%s_segment_%d", ascii_filename.c_str(), (int)segment_number);
}

void AnalyzeSegmentsAndPrintResult(
    const std::vector<base::FilePath>& ref_segments,
    const std::vector<base::FilePath>& actual_segments,
    const base::FilePath& reference_file,
    const std::string& perf_modifier) {
  ASSERT_GT(ref_segments.size(), 0u)
      << "Failed to split reference file on silence; sox is likely broken.";
  ASSERT_EQ(ref_segments.size(), actual_segments.size())
      << "The recording did not result in the same number of audio segments "
      << "after on splitting on silence; WebRTC must have deformed the audio "
      << "too much.";

  for (size_t i = 0; i < ref_segments.size(); i++) {
    float difference_in_decibel = AnalyzeOneSegment(ref_segments[i],
                                                    actual_segments[i],
                                                    i);
    std::string trace_name = MakeTraceName(reference_file, i);
    perf_test::PrintResult("agc_energy_diff", perf_modifier, trace_name,
                           difference_in_decibel, "dB", false);
  }
}

void ComputeAndPrintPesqResults(const base::FilePath& reference_file,
                                const base::FilePath& recorded_output_path,
                                const std::string& perf_modifier) {
  base::FilePath trimmed_reference = CreateTemporaryWaveFile();
  base::FilePath trimmed_recording = CreateTemporaryWaveFile();

  ASSERT_TRUE(RemoveSilence(reference_file, trimmed_reference));
  ASSERT_TRUE(RemoveSilence(recorded_output_path, trimmed_recording));

  std::string raw_mos;
  std::string mos_lqo;
  bool succeeded = RunPesq(trimmed_reference, trimmed_recording, 16000,
                           &raw_mos, &mos_lqo);
  EXPECT_TRUE(succeeded) << "Failed to run PESQ.";
  if (succeeded) {
    perf_test::PrintResult(
        "audio_pesq", perf_modifier, "raw_mos", raw_mos, "score", true);
    perf_test::PrintResult(
        "audio_pesq", perf_modifier, "mos_lqo", mos_lqo, "score", true);
  }

  DeleteFileUnlessTestFailed(trimmed_reference, false);
  DeleteFileUnlessTestFailed(trimmed_recording, false);
}

}  // namespace

// Sets up a two-way WebRTC call and records its output to
// |recorded_output_path|, using getUserMedia.
//
// |reference_file| should have at least five seconds of silence in the
// beginning: otherwise all the reference audio will not be picked up by the
// recording. Note that the reference file will start playing as soon as the
// audio device is up following the getUserMedia call in the left tab. The time
// it takes to negotiate a call isn't deterministic, but five seconds should be
// plenty of time. Similarly, the recording time should be enough to catch the
// whole reference file. If you then silence-trim the reference file and actual
// file, you should end up with two time-synchronized files.
void MAYBE_WebRtcAudioQualityBrowserTest::SetupAndRecordAudioCall(
    const base::FilePath& reference_file,
    const base::FilePath& recorded_output_path,
    const std::string& constraints) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(test::HasReferenceFilesInCheckout());

  ConfigureFakeDeviceToPlayFile(reference_file);

  // Create a two-way call. Mute one of the receivers though; that way it will
  // be receiving audio bytes, but we will not be playing out of both elements.
  GURL test_page = embedded_test_server()->GetURL(kWebRtcAudioTestHtmlPage);
  content::WebContents* left_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(test_page, constraints);
  SetupPeerconnectionWithLocalStream(left_tab);
  MuteMediaElement("remote-view", left_tab);

  content::WebContents* right_tab =
      OpenPageAndGetUserMediaInNewTabWithConstraints(test_page, constraints);
  SetupPeerconnectionWithLocalStream(right_tab);

  NegotiateCall(left_tab, right_tab);

  EXPECT_EQ(
      "ok-capturing",
      ExecuteJavascript(
          base::StringPrintf("startAudioCapture(%d, \"%s\");",
                             kCaptureDurationInSeconds, kWebmRecordingFilename),
          right_tab));

  EXPECT_TRUE(test::PollingWaitUntil("testIsDoneCapturing();", "true",
                                     right_tab, kPollingIntervalInMs));

  HangUp(left_tab);

  RunWebmToWavConverter(webm_recorded_output_filename_, recorded_output_path);
  EXPECT_TRUE(base::DieFileDie(webm_recorded_output_filename_, false));

  DVLOG(0) << "Done recording to " << recorded_output_path.MaybeAsASCII();
}

void MAYBE_WebRtcAudioQualityBrowserTest::TestWithFakeDeviceGetUserMedia(
    const std::string& constraints,
    const std::string& perf_modifier) {
  if (OnWin8()) {
    // http://crbug.com/379798.
    LOG(ERROR) << "This test is not implemented for Windows XP/Win8.";
    return;
  }

  base::FilePath reference_file =
      test::GetReferenceFilesDir().Append(kReferenceFile);
  base::FilePath recorded_output_path = CreateTemporaryWaveFile();

  ASSERT_NO_FATAL_FAILURE(SetupAndRecordAudioCall(
      reference_file, recorded_output_path, constraints));

  ComputeAndPrintPesqResults(reference_file, recorded_output_path,
                             perf_modifier);
  DeleteFileUnlessTestFailed(recorded_output_path, false);
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioQualityBrowserTest,
                       MANUAL_TestCallQualityWithAudioFromFakeDevice) {
  TestWithFakeDeviceGetUserMedia(kAudioOnlyCallConstraints, "_getusermedia");
}

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioQualityBrowserTest,
                       MANUAL_TestCallQualityWithAudioFromWebAudio) {
  if (OnWin8()) {
    // http://crbug.com/379798.
    LOG(ERROR) << "This test is not implemented for Windows XP/Win8.";
    return;
  }
  ASSERT_TRUE(test::HasReferenceFilesInCheckout());
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* left_tab =
      OpenPageWithoutGetUserMedia(kWebRtcAudioTestHtmlPage);
  content::WebContents* right_tab =
      OpenPageWithoutGetUserMedia(kWebRtcAudioTestHtmlPage);

  AddAudioFileToWebAudio(kReferenceFileRelativeUrl, left_tab);

  NegotiateCall(left_tab, right_tab);

  const base::FilePath recorded_output_path = CreateTemporaryWaveFile();

  PlayAudioFileThroughWebAudio(left_tab);

  EXPECT_EQ(
      "ok-capturing",
      ExecuteJavascript(
          base::StringPrintf("startAudioCapture(%d, \"%s\");",
                             kCaptureDurationInSeconds, kWebmRecordingFilename),
          right_tab));

  EXPECT_TRUE(test::PollingWaitUntil("testIsDoneCapturing();", "true",
                                     right_tab, kPollingIntervalInMs));

  HangUp(left_tab);

  RunWebmToWavConverter(webm_recorded_output_filename_, recorded_output_path);
  EXPECT_TRUE(base::DieFileDie(webm_recorded_output_filename_, false));

  DVLOG(0) << "Done recording to " << recorded_output_path.MaybeAsASCII();

  // Compare with the reference file on disk (this is the same file we played
  // through WebAudio earlier).
  base::FilePath reference_file =
      test::GetReferenceFilesDir().Append(kReferenceFile);
  ComputeAndPrintPesqResults(reference_file, recorded_output_path, "_webaudio");
}

/**
 * The auto gain control test plays a file into the fake microphone. Then it
 * sets up a one-way WebRTC call with audio only and records Chrome's output on
 * the receiving side using the audio loopback provided by the quality test
 * (see the class comments for more details).
 *
 * Then both the recording and reference file are split on silence. This creates
 * a number of segments with speech in them. The reason for this is to provide
 * a kind of synchronization mechanism so the start of each speech segment is
 * compared to the start of the corresponding speech segment. This is because we
 * will experience inevitable clock drift between the system clock (which runs
 * the fake microphone) and the sound card (which runs play-out). Effectively
 * re-synchronizing on each segment mitigates this.
 *
 * The silence splitting is inherently sensitive to the sound file we run on.
 * Therefore the reference file must have at least 500 ms of pure silence
 * between speech segments; the test will fail if the output produces more
 * segments than the reference.
 *
 * The test reports the difference in decibel between the reference and output
 * file per 10 ms interval in each speech segment. A value of 6 means the
 * output was 6 dB louder than the reference, presumably because the AGC applied
 * gain to the signal.
 *
 * The test only exercises digital AGC for now.
 *
 * We record in CD format here (44.1 kHz) because that's what the fake input
 * device currently supports, and we want to be able to compare directly. See
 * http://crbug.com/421054.
 */
void MAYBE_WebRtcAudioQualityBrowserTest::TestAutoGainControl(
    const base::FilePath::StringType& reference_filename,
    const std::string& constraints,
    const std::string& perf_modifier) {
  if (OnWin8()) {
    // http://crbug.com/379798.
    LOG(ERROR) << "This test is not implemented for Windows XP/Win8.";
    return;
  }
  base::FilePath reference_file =
      test::GetReferenceFilesDir().Append(reference_filename);
  base::FilePath recorded_output_path = CreateTemporaryWaveFile();

  ASSERT_NO_FATAL_FAILURE(SetupAndRecordAudioCall(
      reference_file, recorded_output_path, constraints));

  base::ScopedTempDir split_ref_files;
  ASSERT_TRUE(split_ref_files.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(
      SplitFileOnSilenceIntoDir(reference_file, split_ref_files.GetPath()));
  std::vector<base::FilePath> ref_segments =
      ListWavFilesInDir(split_ref_files.GetPath());

  base::ScopedTempDir split_actual_files;
  ASSERT_TRUE(split_actual_files.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(SplitFileOnSilenceIntoDir(
      recorded_output_path, split_actual_files.GetPath()));

  // Keep the recording and split files if the analysis fails.
  base::FilePath actual_files_dir = split_actual_files.Take();
  std::vector<base::FilePath> actual_segments =
      ListWavFilesInDir(actual_files_dir);

  AnalyzeSegmentsAndPrintResult(
      ref_segments, actual_segments, reference_file, perf_modifier);

  DeleteFileUnlessTestFailed(recorded_output_path, false);
  DeleteFileUnlessTestFailed(actual_files_dir, true);
}

// The AGC should apply non-zero gain here.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioQualityBrowserTest,
                       MANUAL_TestAutoGainControlOnLowAudio) {
  ASSERT_NO_FATAL_FAILURE(TestAutoGainControl(
      kReferenceFile, kAudioOnlyCallConstraints, "_with_agc"));
}

// Since the AGC is off here there should be no gain at all.
IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAudioQualityBrowserTest,
                       MANUAL_TestAutoGainIsOffWithAudioProcessingOff) {
  const char* kAudioCallWithoutAudioProcessing =
      "{audio: { mandatory: { echoCancellation: false } } }";
  ASSERT_NO_FATAL_FAILURE(TestAutoGainControl(
      kReferenceFile, kAudioCallWithoutAudioProcessing, "_no_agc"));
}
