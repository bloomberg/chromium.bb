// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_browser_test.h"

namespace base {
class CommandLine;
}

namespace content {

// Contains stuff WebRTC browsertests have in common.
class WebRtcContentBrowserTest: public ContentBrowserTest {
 public:
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void SetUp() OVERRIDE;

 protected:
  // Executes |javascript|. The script is required to use
  // window.domAutomationController.send to send a string value back to here.
  std::string ExecuteJavascriptAndReturnResult(
      const std::string& javascript);

  // Waits for the javascript to return OK via the automation controller.
  // If the javascript returns != OK or times out, we fail the test.
  void ExecuteJavascriptAndWaitForOk(const std::string& javascript);

  // Generates javascript code for a getUserMedia call.
  std::string GenerateGetUserMediaCall(const char* function_name,
                                       int min_width,
                                       int max_width,
                                       int min_height,
                                       int max_height,
                                       int min_frame_rate,
                                       int max_frame_rate) const;
};

}  // namespace content
