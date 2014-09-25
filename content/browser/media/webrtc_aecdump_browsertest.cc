// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "media/audio/audio_manager.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const int kExpectedConsumerId = 0;

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

}  // namespace

namespace content {

class WebRtcAecDumpBrowserTest : public WebRtcContentBrowserTest {
 public:
  WebRtcAecDumpBrowserTest() {}
  virtual ~WebRtcAecDumpBrowserTest() {}
};

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAecDump DISABLED_CallWithAecDump
#elif defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_CallWithAecDump DISABLED_CallWithAecDump
#elif defined(OS_WIN) && !defined(NDEBUG)
// Flaky on Webkit Win7 Debug bot: http://crbug.com/417756
#define MAYBE_CallWithAecDump DISABLED_CallWithAecDump
#else
#define MAYBE_CallWithAecDump CallWithAecDump
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that a non-empty AEC dump file
// exists. The AEC dump is enabled through webrtc-internals. The HTML and
// Javascript is bypassed since it would trigger a file picker dialog. Instead,
// the dialog callback FileSelected() is invoked directly. In fact, there's
// never a webrtc-internals page opened at all since that's not needed.
IN_PROC_BROWSER_TEST_F(WebRtcAecDumpBrowserTest, MAYBE_CallWithAecDump) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));
  base::DeleteFile(dump_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  DisableOpusIfOnAndroid();
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  EXPECT_FALSE(base::PathExists(dump_file));

  // Add file extensions that we expect to be added. The dump name will be
  // <temporary path>.<render process id>.<consumer id>, for example
  // "/tmp/.com.google.Chrome.Z6UC3P.12345.0".
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  dump_file = dump_file.AddExtension(IntToStringType(render_process_id))
                       .AddExtension(IntToStringType(kExpectedConsumerId));

  EXPECT_TRUE(base::PathExists(dump_file));
  int64 file_size = 0;
  EXPECT_TRUE(base::GetFileSize(dump_file, &file_size));
  EXPECT_GT(file_size, 0);

  base::DeleteFile(dump_file, false);
}

// TODO(grunell): Add test for multiple dumps when re-use of
// MediaStreamAudioProcessor in AudioCapturer has been removed.

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAecDumpEnabledThenDisabled DISABLED_CallWithAecDumpEnabledThenDisabled
#elif defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_CallWithAecDumpEnabledThenDisabled DISABLED_CallWithAecDumpEnabledThenDisabled
#else
#define MAYBE_CallWithAecDumpEnabledThenDisabled CallWithAecDumpEnabledThenDisabled
#endif

// As above, but enable and disable dump before starting a call. The file should
// be created, but should be empty.
IN_PROC_BROWSER_TEST_F(WebRtcAecDumpBrowserTest,
                       MAYBE_CallWithAecDumpEnabledThenDisabled) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));
  base::DeleteFile(dump_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab, then disabling it.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);
  WebRTCInternals::GetInstance()->DisableAecDump();

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  DisableOpusIfOnAndroid();
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  // Add file extensions that we expect to be added.
  base::ProcessId render_process_id = base::kNullProcessId;
  EXPECT_TRUE(GetRenderProcessHostId(&render_process_id));
  dump_file = dump_file.AddExtension(IntToStringType(render_process_id))
                       .AddExtension(IntToStringType(kExpectedConsumerId));

  EXPECT_FALSE(base::PathExists(dump_file));

  base::DeleteFile(dump_file, false);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_TwoCallsWithAecDump DISABLED_TwoCallsWithAecDump
#elif defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_TwoCallsWithAecDump DISABLED_TwoCallsWithAecDump
#else
#define MAYBE_TwoCallsWithAecDump TwoCallsWithAecDump
#endif

IN_PROC_BROWSER_TEST_F(WebRtcAecDumpBrowserTest, MAYBE_TwoCallsWithAecDump) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  // Create a second window.
  Shell* shell2 = CreateBrowser();
  NavigateToURL(shell2, GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));
  base::DeleteFile(dump_file, false);

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));

  NavigateToURL(shell(), url);
  NavigateToURL(shell2, url);
  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell2->web_contents(),
      "call({video: true, audio: true});",
      &result));
  ASSERT_STREQ("OK", result.c_str());

  EXPECT_FALSE(base::PathExists(dump_file));

  RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  for (; !it.IsAtEnd(); it.Advance()) {
    base::ProcessId render_process_id =
        base::GetProcId(it.GetCurrentValue()->GetHandle());
    EXPECT_NE(base::kNullProcessId, render_process_id);

    // Add file extensions that we expect to be added.
    base::FilePath unique_dump_file =
        dump_file.AddExtension(IntToStringType(render_process_id))
                 .AddExtension(IntToStringType(kExpectedConsumerId));

    EXPECT_TRUE(base::PathExists(unique_dump_file));
    int64 file_size = 0;
    EXPECT_TRUE(base::GetFileSize(unique_dump_file, &file_size));
    EXPECT_GT(file_size, 0);

    base::DeleteFile(unique_dump_file, false);
  }
}

}  // namespace content
