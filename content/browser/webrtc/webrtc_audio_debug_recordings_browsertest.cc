// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace {

const int kExpectedConsumerId = 1;
const int kExpectedInputStreamId = 1;
const int kExpectedFirstOutputStreamId = 1;

const int kWaveHeaderSizeBytes = 44;

const base::FilePath::CharType kBaseFilename[] =
    FILE_PATH_LITERAL("audio_debug");
const base::FilePath::CharType kWaveExtension[] = FILE_PATH_LITERAL("wav");

// Get the ID for the render process host when there should only be one.
bool GetRenderProcessHostId(base::ProcessId* id) {
  content::RenderProcessHost::iterator it(
      content::RenderProcessHost::AllHostsIterator());
  *id = base::GetProcId(it.GetCurrentValue()->GetHandle());
  EXPECT_NE(base::kNullProcessId, *id);
  if (*id == base::kNullProcessId)
    return false;
  it.Advance();
  EXPECT_TRUE(it.IsAtEnd());
  return it.IsAtEnd();
}

// Get the expected AEC dump file name. The name will be
// <temporary path>.<render process id>.aec_dump.<consumer id>, for example
// "/tmp/.com.google.Chrome.Z6UC3P.12345.aec_dump.1".
base::FilePath GetExpectedAecDumpFileName(const base::FilePath& base_file_path,
                                          int render_process_id) {
  return base_file_path.AddExtension(IntToStringType(render_process_id))
      .AddExtension(FILE_PATH_LITERAL("aec_dump"))
      .AddExtension(IntToStringType(kExpectedConsumerId));
}

// Get the expected input audio file name. The name will be
// <temporary path>.<render process id>.source_input.<stream id>.wav, for
// example "/tmp/.com.google.Chrome.Z6UC3P.12345.source_input.1.wav".
base::FilePath GetExpectedInputAudioFileName(
    const base::FilePath& base_file_path,
    int render_process_id) {
  return base_file_path.AddExtension(IntToStringType(render_process_id))
      .AddExtension(FILE_PATH_LITERAL("source_input"))
      .AddExtension(IntToStringType(kExpectedInputStreamId))
      .AddExtension(kWaveExtension);
}

// Get the expected output audio file name. The name will be
// <temporary path>.output.<running stream id>.wav, for example
// "/tmp/.com.google.Chrome.Z6UC3P.output.1.wav".
base::FilePath GetExpectedOutputAudioFileName(
    const base::FilePath& base_file_path,
    int id) {
  return base_file_path.AddExtension(FILE_PATH_LITERAL("output"))
      .AddExtension(IntToStringType(id))
      .AddExtension(kWaveExtension);
}

}  // namespace

namespace content {

class WebRtcAudioDebugRecordingsBrowserTest
    : public WebRtcContentBrowserTestBase {
 public:
  WebRtcAudioDebugRecordingsBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }
  ~WebRtcAudioDebugRecordingsBrowserTest() override {}
};

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_CallWithAudioDebugRecordings DISABLED_CallWithAudioDebugRecordings
#elif defined(OS_ANDROID)
// Renderer crashes on Android M. https://crbug.com/535728.
#define MAYBE_CallWithAudioDebugRecordings DISABLED_CallWithAudioDebugRecordings
#else
#define MAYBE_CallWithAudioDebugRecordings CallWithAudioDebugRecordings
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that non-empty audio debug
// recording files exist. The recording is enabled through webrtc-internals. The
// HTML and Javascript is bypassed since it would trigger a file picker dialog.
// Instead, the dialog callback FileSelected() is invoked directly. In fact,
// there's never a webrtc-internals page opened at all since that's not needed.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_CallWithAudioDebugRecordings) {
  if (!HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  bool prev_io_allowed = base::ThreadRestrictions::SetIOAllowed(true);

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  // Create a temp directory and setup base file path.
  base::FilePath temp_dir_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir_path));
  base::FilePath base_file_path = temp_dir_path.Append(kBaseFilename);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling audio debug recordings in that tab.
  WebRTCInternals::GetInstance()->FileSelected(base_file_path, -1, NULL);

  // Make a call.
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");
  ExecuteJavascriptAndWaitForOk("hangup();");

  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  // Verify that the expected AEC dump file exists and contains some data.
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  base::FilePath file_path =
      GetExpectedAecDumpFileName(base_file_path, render_process_id);
  EXPECT_TRUE(base::PathExists(file_path));
  int64_t file_size = 0;
  EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
  EXPECT_GT(file_size, 0);
  EXPECT_TRUE(base::DeleteFile(file_path, false));

  // Verify that the expected input audio file exists and contains some data.
  file_path = GetExpectedInputAudioFileName(base_file_path, render_process_id);
  EXPECT_TRUE(base::PathExists(file_path));
  file_size = 0;
  EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
  EXPECT_GT(file_size, kWaveHeaderSizeBytes);
  EXPECT_TRUE(base::DeleteFile(file_path, false));

  // Verify that the expected output audio files exists and contains some data.
  // Two files are expected, one for each peer in the call.
  for (int i = 0; i < 2; ++i) {
    file_path = GetExpectedOutputAudioFileName(
        base_file_path, kExpectedFirstOutputStreamId + i);
    EXPECT_TRUE(base::PathExists(file_path));
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, kWaveHeaderSizeBytes);
    EXPECT_TRUE(base::DeleteFile(file_path, false));
  }

  // Verify that no other files exist and remove temp dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_path));
  EXPECT_TRUE(base::DeleteFile(temp_dir_path, false));

  base::ThreadRestrictions::SetIOAllowed(prev_io_allowed);
}

// TODO(grunell): Add test for multiple dumps when re-use of
// MediaStreamAudioProcessor in AudioCapturer has been removed.

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled \
  DISABLED_CallWithAudioDebugRecordingsEnabledThenDisabled
#else
#define MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled \
  CallWithAudioDebugRecordingsEnabledThenDisabled
#endif

// As above, but enable and disable recordings before starting a call. No files
// should be created.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_CallWithAudioDebugRecordingsEnabledThenDisabled) {
  if (!HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  bool prev_io_allowed = base::ThreadRestrictions::SetIOAllowed(true);

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  // Create a temp directory and setup base file path.
  base::FilePath temp_dir_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir_path));
  base::FilePath base_file_path = temp_dir_path.Append(kBaseFilename);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling audio debug recordings in that tab, then disabling it.
  WebRTCInternals::GetInstance()->FileSelected(base_file_path, -1, NULL);
  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  // Make a call.
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");
  ExecuteJavascriptAndWaitForOk("hangup();");

  // Verify that no files exist and remove temp dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_path));
  EXPECT_TRUE(base::DeleteFile(temp_dir_path, false));

  base::ThreadRestrictions::SetIOAllowed(prev_io_allowed);
}

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_TwoCallsWithAudioDebugRecordings \
  DISABLED_TwoCallsWithAudioDebugRecordings
#elif defined(OS_ANDROID)
// Renderer crashes on Android M. https://crbug.com/535728.
#define MAYBE_TwoCallsWithAudioDebugRecordings \
  DISABLED_TwoCallsWithAudioDebugRecordings
#else
#define MAYBE_TwoCallsWithAudioDebugRecordings TwoCallsWithAudioDebugRecordings
#endif

// Same test as CallWithAudioDebugRecordings, but does two parallel calls.
IN_PROC_BROWSER_TEST_F(WebRtcAudioDebugRecordingsBrowserTest,
                       MAYBE_TwoCallsWithAudioDebugRecordings) {
  if (!HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  bool prev_io_allowed = base::ThreadRestrictions::SetIOAllowed(true);

  ASSERT_TRUE(embedded_test_server()->Start());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  // Create a second window.
  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell2, GURL(""));

  // Create a temp directory and setup base file path.
  base::FilePath temp_dir_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(base::FilePath::StringType(), &temp_dir_path));
  base::FilePath base_file_path = temp_dir_path.Append(kBaseFilename);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling audio debug recordings in that tab.
  WebRTCInternals::GetInstance()->FileSelected(base_file_path, -1, NULL);

  // Make the calls.
  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  NavigateToURL(shell2, url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell2, "call({video: true, audio: true});", &result));
  ASSERT_STREQ("OK", result.c_str());

  ExecuteJavascriptAndWaitForOk("hangup();");
  EXPECT_TRUE(ExecuteScriptAndExtractString(shell2, "hangup();", &result));
  ASSERT_STREQ("OK", result.c_str());

  WebRTCInternals::GetInstance()->DisableAudioDebugRecordings();

  RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  base::FilePath file_path;
  int64_t file_size = 0;

  for (; !it.IsAtEnd(); it.Advance()) {
    base::ProcessId render_process_id =
        base::GetProcId(it.GetCurrentValue()->GetHandle());
    EXPECT_NE(base::kNullProcessId, render_process_id);

    // Verify that the expected AEC dump file exists and contains some data.
    file_path = GetExpectedAecDumpFileName(base_file_path, render_process_id);
    EXPECT_TRUE(base::PathExists(file_path));
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, 0);
    EXPECT_TRUE(base::DeleteFile(file_path, false));

    // Verify that the expected input audio file exists and contains some data.
    file_path =
        GetExpectedInputAudioFileName(base_file_path, render_process_id);
    EXPECT_TRUE(base::PathExists(file_path));
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, kWaveHeaderSizeBytes);
    EXPECT_TRUE(base::DeleteFile(file_path, false));
  }

  // Verify that the expected output audio files exists and contains some data.
  // Four files are expected, one for each peer in each call. (Two calls * two
  // peers.)
  for (int i = 0; i < 4; ++i) {
    file_path = GetExpectedOutputAudioFileName(
        base_file_path, kExpectedFirstOutputStreamId + i);
    EXPECT_TRUE(base::PathExists(file_path));
    file_size = 0;
    EXPECT_TRUE(base::GetFileSize(file_path, &file_size));
    EXPECT_GT(file_size, kWaveHeaderSizeBytes);
    EXPECT_TRUE(base::DeleteFile(file_path, false));
  }

  // Verify that no other files exist and remove temp dir.
  EXPECT_TRUE(base::IsDirectoryEmpty(temp_dir_path));
  EXPECT_TRUE(base::DeleteFile(temp_dir_path, false));

  base::ThreadRestrictions::SetIOAllowed(prev_io_allowed);
}

}  // namespace content
