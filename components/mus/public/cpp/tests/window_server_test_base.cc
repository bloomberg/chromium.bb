// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/window_server_test_base.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_client.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "services/shell/public/cpp/connector.h"

namespace mus {
namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

}  // namespace

WindowServerTestBase::WindowServerTestBase()
    : most_recent_client_(nullptr),
      window_manager_(nullptr),
      window_manager_delegate_(nullptr),
      window_manager_client_(nullptr),
      window_tree_client_destroyed_(false) {}

WindowServerTestBase::~WindowServerTestBase() {}

// static
bool WindowServerTestBase::DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&TimeoutRunLoop, run_loop.QuitClosure(), &timeout),
      TestTimeouts::action_timeout());

  current_run_loop = &run_loop;
  current_run_loop->Run();
  current_run_loop = nullptr;
  return !timeout;
}

// static
bool WindowServerTestBase::QuitRunLoop() {
  if (!current_run_loop)
    return false;

  current_run_loop->Quit();
  current_run_loop = nullptr;
  return true;
}

void WindowServerTestBase::SetUp() {
  WindowServerShellTestBase::SetUp();

  CreateWindowTreeHost(connector(), this, &host_, this);

  ASSERT_TRUE(DoRunLoopWithTimeout());  // RunLoop should be quit by OnEmbed().
  std::swap(window_manager_, most_recent_client_);
}

bool WindowServerTestBase::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<mojom::WindowTreeClient>(this);
  return true;
}

void WindowServerTestBase::OnEmbed(Window* root) {
  most_recent_client_ = root->window_tree();
  EXPECT_TRUE(QuitRunLoop());
  ASSERT_TRUE(window_manager_client_);
  window_manager_client_->AddActivationParent(root);
}

void WindowServerTestBase::OnWindowTreeClientDestroyed(
    WindowTreeClient* client) {
  window_tree_client_destroyed_ = true;
}

void WindowServerTestBase::OnEventObserved(const ui::Event& event,
                                           Window* target) {}

void WindowServerTestBase::SetWindowManagerClient(WindowManagerClient* client) {
  window_manager_client_ = client;
}

bool WindowServerTestBase::OnWmSetBounds(Window* window, gfx::Rect* bounds) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnWmSetBounds(window, bounds)
             : true;
}

bool WindowServerTestBase::OnWmSetProperty(
    Window* window,
    const std::string& name,
    std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnWmSetProperty(window, name, new_data)
             : true;
}

Window* WindowServerTestBase::OnWmCreateTopLevelWindow(
    std::map<std::string, std::vector<uint8_t>>* properties) {
  return window_manager_delegate_
             ? window_manager_delegate_->OnWmCreateTopLevelWindow(properties)
             : nullptr;
}

void WindowServerTestBase::OnWmClientJankinessChanged(
    const std::set<Window*>& client_windows,
    bool janky) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnWmClientJankinessChanged(client_windows, janky);
}

void WindowServerTestBase::OnWmNewDisplay(Window* window,
                                          const display::Display& display) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnWmNewDisplay(window, display);
}

void WindowServerTestBase::OnAccelerator(uint32_t id, const ui::Event& event) {
  if (window_manager_delegate_)
    window_manager_delegate_->OnAccelerator(id, event);
}

void WindowServerTestBase::Create(shell::Connection* connection,
                                  mojom::WindowTreeClientRequest request) {
  new WindowTreeClient(this, nullptr, std::move(request));
}

}  // namespace mus
