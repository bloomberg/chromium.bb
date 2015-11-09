// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/window_server_test_base.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "mojo/application/public/cpp/application_impl.h"

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
    : most_recent_connection_(nullptr),
      window_manager_(nullptr),
      window_tree_connection_destroyed_(false) {}

WindowServerTestBase::~WindowServerTestBase() {}

// static
bool WindowServerTestBase::DoRunLoopWithTimeout() {
  if (current_run_loop != nullptr)
    return false;

  bool timeout = false;
  base::RunLoop run_loop;
  base::MessageLoop::current()->PostDelayedTask(
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
  ApplicationTestBase::SetUp();

  CreateSingleWindowTreeHost(application_impl(), this, &host_, nullptr,
                             nullptr);

  ASSERT_TRUE(DoRunLoopWithTimeout());  // RunLoop should be quit by OnEmbed().
  std::swap(window_manager_, most_recent_connection_);
}

void WindowServerTestBase::TearDown() {
  ApplicationTestBase::TearDown();
}

mojo::ApplicationDelegate* WindowServerTestBase::GetApplicationDelegate() {
  return this;
}

bool WindowServerTestBase::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mojom::WindowTreeClient>(this);
  return true;
}

void WindowServerTestBase::OnEmbed(Window* root) {
  most_recent_connection_ = root->connection();
  EXPECT_TRUE(QuitRunLoop());
}

void WindowServerTestBase::OnConnectionLost(WindowTreeConnection* connection) {
  window_tree_connection_destroyed_ = true;
}

void WindowServerTestBase::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojom::WindowTreeClient> request) {
  WindowTreeConnection::Create(
      this, request.Pass(),
      WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

}  // namespace mus
