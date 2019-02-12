// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/ws/common/switches.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/window_server_test.mojom-test-utils.h"
#include "services/ws/public/mojom/window_server_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor_switches.h"

class ShortcutViewerBrowserTest : public InProcessBrowserTest {
 public:
  ShortcutViewerBrowserTest() = default;
  ~ShortcutViewerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ws::switches::kUseTestConfig);
    // This test ensures a paint happens, which requires pixel output.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShortcutViewerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ShortcutViewerBrowserTest, Paints) {
  // Start up shortcut_viewer.
  service_manager::Connector* connector =
      content::ServiceManagerConnection::GetForProcess()->GetConnector();
  shortcut_viewer::mojom::ShortcutViewerPtr shortcut_viewer;
  connector->BindInterface(service_manager::ServiceFilter::ByName(
                               shortcut_viewer::mojom::kServiceName),
                           mojo::MakeRequest(&shortcut_viewer));
  shortcut_viewer->Toggle(base::TimeTicks::Now());

  // Connect to WindowService and verify shortcut_viewer painted.
  ws::mojom::WindowServerTestPtr test_interface;
  connector->BindInterface(
      service_manager::ServiceFilter::ByName(ws::mojom::kServiceName),
      mojo::MakeRequest(&test_interface));

  bool success = false;
  ws::mojom::WindowServerTestAsyncWaiter wait_for(test_interface.get());
  wait_for.EnsureClientHasDrawnWindow(shortcut_viewer::mojom::kServiceName,
                                      &success);
  EXPECT_TRUE(success);
}
