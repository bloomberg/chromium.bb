// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ByMove;
using testing::Return;

namespace content {

namespace {

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MockWebContentsDelegate() {}
  ~MockWebContentsDelegate() override {}

  std::unique_ptr<SerialChooser> RunSerialChooser(
      content::RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      content::SerialChooser::Callback callback) override {
    std::move(callback).Run(RunSerialChooserInternal());
    return nullptr;
  }

  MOCK_METHOD0(RunSerialChooserInternal, blink::mojom::SerialPortInfoPtr());
};

}  // namespace

class SerialTest : public ContentBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "Serial");
  }
};

IN_PROC_BROWSER_TEST_F(SerialTest, GetPorts) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  int result;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell(),
      "navigator.serial.getPorts()"
      "    .then(ports => {"
      "        domAutomationController.send(ports.length);"
      "    });",
      &result));
  EXPECT_EQ(0, result);
}

IN_PROC_BROWSER_TEST_F(SerialTest, RequestPort) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  MockWebContentsDelegate delegate;
  shell()->web_contents()->SetDelegate(&delegate);

  auto port = blink::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  EXPECT_CALL(delegate, RunSerialChooserInternal)
      .WillOnce(Return(ByMove(std::move(port))));

  bool result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(shell(),
                                  "navigator.serial.requestPort({})"
                                  "    .then(port => {"
                                  "        domAutomationController.send(true);"
                                  "    }, error => {"
                                  "        domAutomationController.send(false);"
                                  "    });",
                                  &result));
  EXPECT_TRUE(result);
}

}  // namespace content
