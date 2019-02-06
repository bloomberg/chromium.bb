// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/unguessable_token.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/serial_chooser.h"
#include "content/public/browser/serial_delegate.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ByMove;
using testing::Return;

namespace content {

namespace {

class FakeSerialPortManager : public device::mojom::SerialPortManager {
 public:
  FakeSerialPortManager() = default;
  ~FakeSerialPortManager() override = default;

  void AddPort(device::mojom::SerialPortInfoPtr port) {
    base::UnguessableToken token = port->token;
    ports_[token] = std::move(port);
  }

  // device::mojom::SerialPortManager
  void GetDevices(GetDevicesCallback callback) override {
    std::vector<device::mojom::SerialPortInfoPtr> ports;
    for (const auto& map_entry : ports_)
      ports.push_back(map_entry.second.Clone());
    std::move(callback).Run(std::move(ports));
  }

  void GetPort(const base::UnguessableToken& token,
               device::mojom::SerialPortRequest request) override {}

 private:
  std::map<base::UnguessableToken, device::mojom::SerialPortInfoPtr> ports_;

  DISALLOW_COPY_AND_ASSIGN(FakeSerialPortManager);
};

class MockSerialDelegate : public SerialDelegate {
 public:
  MockSerialDelegate() = default;
  ~MockSerialDelegate() override = default;

  std::unique_ptr<SerialChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::SerialPortFilterPtr> filters,
      SerialChooser::Callback callback) override {
    std::move(callback).Run(RunChooserInternal());
    return nullptr;
  }

  MOCK_METHOD0(RunChooserInternal, device::mojom::SerialPortInfoPtr());
  MOCK_METHOD2(HasPortPermission,
               bool(content::RenderFrameHost* frame,
                    const device::mojom::SerialPortInfo& port));
  MOCK_METHOD1(
      GetPortManager,
      device::mojom::SerialPortManager*(content::RenderFrameHost* frame));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSerialDelegate);
};

class TestContentBrowserClient : public ContentBrowserClient {
 public:
  TestContentBrowserClient() = default;
  ~TestContentBrowserClient() override = default;

  MockSerialDelegate& delegate() { return delegate_; }

  SerialDelegate* GetSerialDelegate() override { return &delegate_; }

 private:
  MockSerialDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestContentBrowserClient);
};

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
  FakeSerialPortManager* port_manager() { return &port_manager_; }

 private:
  TestContentBrowserClient test_client_;
  ContentBrowserClient* original_client_ = nullptr;
  FakeSerialPortManager port_manager_;
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

  int result;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      shell(),
      "navigator.serial.getPorts()"
      "    .then(ports => {"
      "        domAutomationController.send(ports.length);"
      "    });",
      &result));
  EXPECT_EQ(2, result);
}

IN_PROC_BROWSER_TEST_F(SerialTest, RequestPort) {
  NavigateToURL(shell(), GetTestUrl(nullptr, "simple_page.html"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  EXPECT_CALL(delegate(), RunChooserInternal)
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
