// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_browser_test.h"

class CommandLine;

namespace content {

// Contains stuff WebRTC browsertests have in common.
class WebRtcContentBrowserTest: public ContentBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;
  virtual void SetUp() OVERRIDE;

 protected:
  // Executes |javascript| and returns after it has been executed.
  bool ExecuteJavascript(const std::string& javascript);

  // Executes |javascript|. The script is required to use
  // window.domAutomationController.send to send a string value back to here.
  std::string ExecuteJavascriptAndReturnResult(
      const std::string& javascript);

  // Waits for the page title to be set to |expected_title|.
  void ExpectTitle(const std::string& expected_title) const;

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
