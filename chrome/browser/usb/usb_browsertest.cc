// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/usb_chooser_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "device/base/mock_device_client.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mock_usb_service.h"
#include "device/usb/public/interfaces/chooser_service.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "services/service_manager/public/cpp/binder_registry.h"

using content::RenderFrameHost;
using device::MockDeviceClient;
using device::MockUsbDevice;

namespace {

class FakeChooserView : public ChooserController::View {
 public:
  explicit FakeChooserView(std::unique_ptr<ChooserController> controller)
      : controller_(std::move(controller)) {
    controller_->set_view(this);
  }

  ~FakeChooserView() override { controller_->set_view(nullptr); }

  void OnOptionsInitialized() override {
    if (controller_->NumOptions())
      controller_->Select({0});
    else
      controller_->Cancel();
    delete this;
  }

  void OnOptionAdded(size_t index) override { NOTREACHED(); }
  void OnOptionRemoved(size_t index) override { NOTREACHED(); }
  void OnOptionUpdated(size_t index) override { NOTREACHED(); }
  void OnAdapterEnabledChanged(bool enabled) override { NOTREACHED(); }
  void OnRefreshStateChanged(bool refreshing) override { NOTREACHED(); }

 private:
  std::unique_ptr<ChooserController> controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeChooserView);
};

class FakeChooserService : public device::mojom::UsbChooserService {
 public:
  static void Create(RenderFrameHost* render_frame_host,
                     const service_manager::BindSourceInfo& source_info,
                     device::mojom::UsbChooserServiceRequest request) {
    mojo::MakeStrongBinding(
        base::MakeUnique<FakeChooserService>(render_frame_host),
        std::move(request));
  }

  explicit FakeChooserService(RenderFrameHost* render_frame_host)
      : render_frame_host_(render_frame_host) {}

  ~FakeChooserService() override {}

  // device::mojom::UsbChooserService:
  void GetPermission(
      std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
      const GetPermissionCallback& callback) override {
    auto chooser_controller = base::MakeUnique<UsbChooserController>(
        render_frame_host_, std::move(device_filters), callback);
    new FakeChooserView(std::move(chooser_controller));
  }

 private:
  RenderFrameHost* const render_frame_host_;

  DISALLOW_COPY_AND_ASSIGN(FakeChooserService);
};

class WebUsbTest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    device_client_.reset(new MockDeviceClient());
    scoped_refptr<MockUsbDevice> mock_device(
        new MockUsbDevice(0, 0, "Test Manufacturer", "Test Device", "123456"));
    device_client_->usb_service()->AddDevice(mock_device);
  }

 private:
  std::unique_ptr<MockDeviceClient> device_client_;
};

IN_PROC_BROWSER_TEST_F(WebUsbTest, RequestAndGetDevices) {
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("localhost", "/simple_page.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  EXPECT_THAT(render_frame_host->GetLastCommittedOrigin().Serialize(),
              testing::StartsWith("http://localhost:"));

  render_frame_host->GetInterfaceRegistry()->AddInterface(
      base::Bind(&FakeChooserService::Create, render_frame_host));

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.getDevices()"
      "    .then(devices => {"
      "        domAutomationController.send(devices.length.toString());"
      "     });",
      &result));
  EXPECT_EQ("0", result);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.requestDevice({ filters: [ { vendorId: 0 } ] })"
      "    .then(device => {"
      "        domAutomationController.send(device.serialNumber);"
      "     });",
      &result));
  EXPECT_EQ("123456", result);

  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "navigator.usb.getDevices()"
      "    .then(devices => {"
      "        domAutomationController.send(devices.length.toString());"
      "     });",
      &result));
  EXPECT_EQ("1", result);
}

}  // namespace
