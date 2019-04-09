// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "content/browser/serial/serial_test_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/serial_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace content {

namespace {

class SerialTest : public ContentBrowserTest {
 public:
  SerialTest() {
    ON_CALL(delegate(), GetPortManager).WillByDefault(Return(&port_manager_));
  }

  ~SerialTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("enable-blink-features", "Serial");
  }

  void SetUpOnMainThread() override {
    original_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDownOnMainThread() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  MockSerialDelegate& delegate() { return test_client_.delegate(); }
  device::FakeSerialPortManager* port_manager() { return &port_manager_; }

 private:
  SerialTestContentBrowserClient test_client_;
  ContentBrowserClient* original_client_ = nullptr;
  device::FakeSerialPortManager port_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialTest, GetPorts) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  // Three ports are added but only two will have permission granted.
  for (size_t i = 0; i < 3; i++) {
    auto port = device::mojom::SerialPortInfo::New();
    port->token = base::UnguessableToken::Create();
    port_manager()->AddPort(std::move(port));
  }

  EXPECT_CALL(delegate(), HasPortPermission(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  EXPECT_EQ(
      2, EvalJs(shell(),
                R"(navigator.serial.getPorts().then(ports => ports.length))"));
}

IN_PROC_BROWSER_TEST_F(SerialTest, RequestPort) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  EXPECT_CALL(delegate(), RunChooserInternal)
      .WillOnce(Return(ByMove(std::move(port))));

  EXPECT_EQ(true, EvalJs(shell(),
                         R"((async () => {
                           let port = await navigator.serial.requestPort({});
                           return port instanceof SerialPort;
                         })())"));
}

}  // namespace content
