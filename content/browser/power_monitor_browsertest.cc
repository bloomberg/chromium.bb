// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/power_monitor_test.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/device/public/interfaces/power_monitor.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace content {

namespace {

void VerifyPowerStateInChildProcess(mojom::PowerMonitorTest* power_monitor_test,
                                    bool expected_state) {
  base::RunLoop run_loop;
  power_monitor_test->QueryNextState(base::Bind(
      [](const base::Closure& quit, bool expected_state,
         bool on_battery_power) {
        EXPECT_EQ(expected_state, on_battery_power);
        quit.Run();
      },
      run_loop.QuitClosure(), expected_state));
  run_loop.Run();
}

class MockPowerMonitorMessageBroadcaster : public device::mojom::PowerMonitor {
 public:
  MockPowerMonitorMessageBroadcaster() = default;
  ~MockPowerMonitorMessageBroadcaster() override = default;

  void Bind(device::mojom::PowerMonitorRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // device::mojom::PowerMonitor:
  void AddClient(
      device::mojom::PowerMonitorClientPtr power_monitor_client) override {
    power_monitor_client->PowerStateChange(on_battery_power_);
    clients_.AddPtr(std::move(power_monitor_client));
  }

  void OnPowerStateChange(bool on_battery_power) {
    on_battery_power_ = on_battery_power;
    clients_.ForAllPtrs(
        [&on_battery_power](device::mojom::PowerMonitorClient* client) {
          client->PowerStateChange(on_battery_power);
        });
  }

 private:
  bool on_battery_power_ = false;

  mojo::BindingSet<device::mojom::PowerMonitor> bindings_;
  mojo::InterfacePtrSet<device::mojom::PowerMonitorClient> clients_;

  DISALLOW_COPY_AND_ASSIGN(MockPowerMonitorMessageBroadcaster);
};

class PowerMonitorTest : public ContentBrowserTest {
 public:
  PowerMonitorTest() = default;

  void SetUpOnMainThread() override {
    // Because Device Service also runs in this process(browser process), we can
    // set our binder to intercept requests for PowerMonitor interface to it.
    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::PowerMonitor::Name_,
        base::Bind(&PowerMonitorTest::BindPowerMonitor,
                   base::Unretained(this)));
  }

  void BindPowerMonitor(const service_manager::BindSourceInfo& source_info,
                        const std::string& interface_name,
                        mojo::ScopedMessagePipeHandle handle) {
    if (source_info.identity.name() == mojom::kRendererServiceName)
      ++request_count_from_renderer_;

    power_monitor_message_broadcaster_.Bind(
        device::mojom::PowerMonitorRequest(std::move(handle)));
  }

 protected:
  int request_count_from_renderer() { return request_count_from_renderer_; }

  void SimulatePowerStateChange(bool on_battery_power) {
    power_monitor_message_broadcaster_.OnPowerStateChange(on_battery_power);
  }

 private:
  int request_count_from_renderer_ = 0;

  MockPowerMonitorMessageBroadcaster power_monitor_message_broadcaster_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitorTest);
};

IN_PROC_BROWSER_TEST_F(PowerMonitorTest, RequestOnceFromOneRendererProcess) {
  ASSERT_EQ(0, request_count_from_renderer());
  ASSERT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));
  EXPECT_EQ(1, request_count_from_renderer());

  mojom::PowerMonitorTestPtr power_monitor_renderer;
  RenderProcessHost* rph =
      shell()->web_contents()->GetMainFrame()->GetProcess();
  BindInterface(rph, &power_monitor_renderer);

  SimulatePowerStateChange(true);
  // Verify renderer process on_battery_power changed to true.
  VerifyPowerStateInChildProcess(power_monitor_renderer.get(), true);

  SimulatePowerStateChange(false);
  // Verify renderer process on_battery_power changed to false.
  VerifyPowerStateInChildProcess(power_monitor_renderer.get(), false);
}

}  //  namespace

}  //  namespace content
