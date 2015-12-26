// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/service_registry.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "device/vibration/vibration_manager.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

// These tests run against a dummy implementation of the VibrationManager
// service. That is, they verify that the service implementation is correctly
// exposed to the renderer, whatever the implementation is.

namespace content {

namespace {

// Global, record milliseconds when Vibrate() got called.
int64_t g_vibrate_milliseconds;
// Global, record whether Cancel() got called.
bool g_cancelled;
// Global, wait for end of execution for VibrationManager API Vibrate().
scoped_refptr<content::MessageLoopRunner> g_wait_vibrate_runner;
// Global, wait for end of execution for VibrationManager API Cancel().
scoped_refptr<content::MessageLoopRunner> g_wait_cancel_runner;

void ResetGlobalValues() {
  g_vibrate_milliseconds = -1;
  g_cancelled = false;

  g_wait_vibrate_runner = new content::MessageLoopRunner();
  g_wait_cancel_runner = new content::MessageLoopRunner();
}

class FakeVibrationManager : public device::VibrationManager {
 public:
  static void Create(mojo::InterfaceRequest<VibrationManager> request) {
    new FakeVibrationManager(std::move(request));
  }

 private:
  FakeVibrationManager(mojo::InterfaceRequest<VibrationManager> request)
      : binding_(this, std::move(request)) {}
  ~FakeVibrationManager() override {}

  void Vibrate(int64_t milliseconds) override {
    g_vibrate_milliseconds = milliseconds;
    g_wait_vibrate_runner->Quit();
  }

  void Cancel() override {
    g_cancelled = true;
    g_wait_cancel_runner->Quit();
  }

  mojo::StrongBinding<VibrationManager> binding_;
};

// Overrides the default service implementation with the test implementation
// declared above.
class TestContentBrowserClient : public ContentBrowserClient {
 public:
  void RegisterRenderProcessMojoServices(ServiceRegistry* registry) override {
    registry->AddService(base::Bind(&FakeVibrationManager::Create));
  }

#if defined(OS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      FileDescriptorInfo* mappings,
      std::map<int, base::MemoryMappedFile::Region>* regions) override {
    ShellContentBrowserClient::Get()->GetAdditionalMappedFilesForChildProcess(
        command_line, child_process_id, mappings, regions);
  }
#endif  // defined(OS_ANDROID)
};

class VibrationManagerIntegrationTest : public ContentBrowserTest {
 public:
  VibrationManagerIntegrationTest() {}

  void SetUpOnMainThread() override {
    old_client_ = SetBrowserClientForTesting(&test_client_);
    ResetGlobalValues();
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(old_client_);
  }

 private:
  TestContentBrowserClient test_client_;
  ContentBrowserClient* old_client_;

  DISALLOW_COPY_AND_ASSIGN(VibrationManagerIntegrationTest);
};

IN_PROC_BROWSER_TEST_F(VibrationManagerIntegrationTest, Vibrate) {
  // From JavaScript call navigator.vibrate(3000),
  // then check the global value g_vibrate_milliseconds.
  ASSERT_EQ(-1, g_vibrate_milliseconds);
  ASSERT_FALSE(g_wait_vibrate_runner->loop_running());

  GURL test_url =
      GetTestUrl("vibration", "vibration_manager_vibrate_test.html");
  shell()->LoadURL(test_url);
  // Wait until VibrationManager::Vibrate() got called.
  g_wait_vibrate_runner->Run();

  EXPECT_EQ(3000, g_vibrate_milliseconds);
}

IN_PROC_BROWSER_TEST_F(VibrationManagerIntegrationTest, Cancel) {
  // From JavaScript call navigator.vibrate(0),
  // then check the global value g_cancelled.
  ASSERT_FALSE(g_cancelled);
  ASSERT_FALSE(g_wait_cancel_runner->loop_running());

  GURL test_url = GetTestUrl("vibration", "vibration_manager_cancel_test.html");
  shell()->LoadURL(test_url);
  // Wait until VibrationManager::Cancel() got called.
  g_wait_cancel_runner->Run();

  EXPECT_TRUE(g_cancelled);
}

}  //  namespace

}  //  namespace content
