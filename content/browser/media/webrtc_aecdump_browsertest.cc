// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

// This tests AEC dump enabled using the command line flag. It does not test AEC
// dump enabled using webrtc-internals (that's tested in webrtc_browsertest.cc).
class WebrtcAecDumpBrowserTest : public ContentBrowserTest {
 public:
  WebrtcAecDumpBrowserTest() {}
  virtual ~WebrtcAecDumpBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    // Find a safe place to write the dump to.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(CreateTemporaryFileInDir(temp_dir_.path(), &dump_file_));

    ContentBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We need fake devices in this test since we want to run on naked VMs. We
    // assume these switches are set by default in content_browsertests.
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeUIForMediaStream));

    // The video playback will not work without a GPU, so force its use here.
    // This may not be available on all VMs though.
    command_line->AppendSwitch(switches::kUseGpuInTests);

    // Enable AEC dump with the command line flag.
    command_line->AppendSwitchPath(switches::kEnableWebRtcAecRecordings,
                                   dump_file_);
  }

 protected:
  bool ExecuteJavascript(const std::string& javascript) {
    return ExecuteScript(shell()->web_contents(), javascript);
  }

  void ExpectTitle(const std::string& expected_title) const {
    base::string16 expected_title16(ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath dump_file_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebrtcAecDumpBrowserTest);
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
IN_PROC_BROWSER_TEST_F(WebrtcAecDumpBrowserTest, MAYBE_CallWithAecDump) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);

  EXPECT_TRUE(ExecuteJavascript("call({video: true, audio: true});"));
  ExpectTitle("OK");

  EXPECT_TRUE(base::PathExists(dump_file_));
  int64 file_size = 0;
  EXPECT_TRUE(base::GetFileSize(dump_file_, &file_size));
  EXPECT_GT(file_size, 0);
}

}  // namespace content
