// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/platform_file.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/trace_event_analyzer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

// This tests AEC dump enabled using the command line flag. It does not test AEC
// dump enabled using webrtc-internals (that's tested in webrtc_browsertest.cc).
class WebRtcAecDumpBrowserTest : public WebRtcContentBrowserTest {
 public:
  WebRtcAecDumpBrowserTest() {}
  virtual ~WebRtcAecDumpBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    // Find a safe place to write the dump to.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir_.path(), &dump_file_));

    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebRtcContentBrowserTest::SetUpCommandLine(command_line);

    // Enable AEC dump with the command line flag.
    command_line->AppendSwitchPath(switches::kEnableWebRtcAecRecordings,
                                   dump_file_);
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath dump_file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebRtcAecDumpBrowserTest);
};

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAecDump DISABLED_CallWithAecDump
#else
#define MAYBE_CallWithAecDump CallWithAecDump
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that a non-empty AEC dump file
// exists.
IN_PROC_BROWSER_TEST_F(WebRtcAecDumpBrowserTest, MAYBE_CallWithAecDump) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

#if defined (OS_ANDROID)
  // Always force iSAC 16K on Android for now (Opus is broken).
  EXPECT_EQ("isac-forced",
            ExecuteJavascriptAndReturnResult("forceIsac16KInSdp();"));
#endif

  ExecuteJavascriptAndWaitForOk("call({video: true, audio: true});");

  EXPECT_TRUE(base::PathExists(dump_file_));
  int64 file_size = 0;
  EXPECT_TRUE(base::GetFileSize(dump_file_, &file_size));
  EXPECT_GT(file_size, 0);
}

}  // namespace content
