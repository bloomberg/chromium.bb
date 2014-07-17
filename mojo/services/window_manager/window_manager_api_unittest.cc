// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/public/interfaces/window_manager/window_manager.mojom.h"
#include "mojo/shell/shell_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

// Callback from EmbedRoot(). |result| is the result of the
// Embed() call and |run_loop| the nested RunLoop.
void ResultCallback(bool* result_cache, base::RunLoop* run_loop, bool result) {
  *result_cache = result;
  run_loop->Quit();
}

// Responsible for establishing the initial ViewManagerService connection.
// Blocks until result is determined.
bool EmbedRoot(view_manager::ViewManagerInitService* view_manager_init,
               const std::string& url) {
  bool result = false;
  base::RunLoop run_loop;
  view_manager_init->EmbedRoot(url,
                               base::Bind(&ResultCallback, &result, &run_loop));
  run_loop.Run();
  return result;
}

void OpenWindowCallback(view_manager::Id* id,
                        base::RunLoop* run_loop,
                        view_manager::Id window_id) {
  *id = window_id;
  run_loop->Quit();
}

view_manager::Id OpenWindow(WindowManagerService* window_manager) {
  base::RunLoop run_loop;
  view_manager::Id id;
  window_manager->OpenWindow(
      base::Bind(&OpenWindowCallback, &id, &run_loop));
  run_loop.Run();
  return id;
}

class TestWindowManagerClient : public WindowManagerClient {
 public:
  explicit TestWindowManagerClient(base::RunLoop* run_loop)
      : run_loop_(run_loop) {}
  virtual ~TestWindowManagerClient() {}

 private:
  // Overridden from WindowManagerClient:
  virtual void OnWindowManagerReady() MOJO_OVERRIDE {
    run_loop_->Quit();
  }
  virtual void OnCaptureChanged(
      view_manager::Id old_capture_node_id,
      view_manager::Id new_capture_node_id) MOJO_OVERRIDE {}

  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerClient);
};

}  // namespace

class WindowManagerApiTest : public testing::Test {
 public:
  WindowManagerApiTest() {}
  virtual ~WindowManagerApiTest() {}

 protected:
  WindowManagerServicePtr window_manager_;

 private:
  // Overridden from testing::Test:
  virtual void SetUp() MOJO_OVERRIDE {
    test_helper_.Init();
    test_helper_.service_manager()->ConnectToService(
        GURL("mojo:mojo_view_manager"),
        &view_manager_init_);
    ASSERT_TRUE(EmbedRoot(view_manager_init_.get(),
                          "mojo:mojo_core_window_manager"));
    ConnectToWindowManager();
  }
  virtual void TearDown() MOJO_OVERRIDE {}

  void ConnectToWindowManager() {
    test_helper_.service_manager()->ConnectToService(
        GURL("mojo:mojo_core_window_manager"),
        &window_manager_);
    base::RunLoop connect_loop;
    window_manager_client_ = new TestWindowManagerClient(&connect_loop);
    window_manager_.set_client(window_manager_client_);
    connect_loop.Run();
  }

  base::MessageLoop loop_;
  shell::ShellTestHelper test_helper_;
  view_manager::ViewManagerInitServicePtr view_manager_init_;
  TestWindowManagerClient* window_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerApiTest);
};

TEST_F(WindowManagerApiTest, OpenWindow) {
  OpenWindow(window_manager_.get());
}

}  // namespace mojo
