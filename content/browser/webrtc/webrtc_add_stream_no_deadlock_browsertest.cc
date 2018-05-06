// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"

namespace content {

#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
// Renderer crashes under Android ASAN: https://crbug.com/408496.
#define MAYBE_WebRtcAddStreamNoDeadlockBrowserTest \
  DISABLED_WebRtcAddStreamNoDeadlockBrowserTest
#else
#define MAYBE_WebRtcAddStreamNoDeadlockBrowserTest \
  WebRtcAddStreamNoDeadlockBrowserTest
#endif

// This test sets up a peer connection in a specific way which previously
// resulted in a deadlock. The test succeeds if the JavaScript completes
// without errors. If the deadlock is re-introduced the test will time out.
// This is a regression test for https://crbug.com/736725.
class MAYBE_WebRtcAddStreamNoDeadlockBrowserTest
    : public WebRtcContentBrowserTestBase {
 public:
  MAYBE_WebRtcAddStreamNoDeadlockBrowserTest() {}
  ~MAYBE_WebRtcAddStreamNoDeadlockBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebRtcContentBrowserTestBase::SetUpCommandLine(command_line);
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
  }

 protected:
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    MakeTypicalCall(javascript, "/media/peerconnection-add-stream.html");
  }
};

IN_PROC_BROWSER_TEST_F(MAYBE_WebRtcAddStreamNoDeadlockBrowserTest,
                       AddStreamDoesNotCauseDeadlock) {
  MakeTypicalPeerConnectionCall("runTest();");
}
}  // namespace content
