// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/tests/view_manager_test_base.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "mojo/application/public/cpp/application_impl.h"

namespace mojo {
namespace {

base::RunLoop* current_run_loop = nullptr;

void TimeoutRunLoop(const base::Closure& timeout_task, bool* timeout) {
  CHECK(current_run_loop);
  *timeout = true;
  timeout_task.Run();
}

}  // namespace

ViewManagerTestBase::ViewManagerTestBase()
    : most_recent_connection_(nullptr),
      window_manager_(nullptr),
      view_tree_connection_destroyed_(false) {}

ViewManagerTestBase::~ViewManagerTestBase() {}

// static
bool ViewManagerTestBase::DoRunLoopWithTimeout() {
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
bool ViewManagerTestBase::QuitRunLoop() {
  if (!current_run_loop)
    return false;

  current_run_loop->Quit();
  current_run_loop = nullptr;
  return true;
}

void ViewManagerTestBase::SetUp() {
  ApplicationTestBase::SetUp();

  CreateSingleViewTreeHost(application_impl(), this, &host_);

  ASSERT_TRUE(DoRunLoopWithTimeout());  // RunLoop should be quit by OnEmbed().
  std::swap(window_manager_, most_recent_connection_);
}

void ViewManagerTestBase::TearDown() {
  ApplicationTestBase::TearDown();
}

ApplicationDelegate* ViewManagerTestBase::GetApplicationDelegate() {
  return this;
}

bool ViewManagerTestBase::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<ViewTreeClient>(this);
  return true;
}

void ViewManagerTestBase::OnEmbed(View* root) {
  most_recent_connection_ = root->connection();
  EXPECT_TRUE(QuitRunLoop());
}

void ViewManagerTestBase::OnConnectionLost(ViewTreeConnection* connection) {
  view_tree_connection_destroyed_ = true;
}

void ViewManagerTestBase::Create(ApplicationConnection* connection,
                                 InterfaceRequest<ViewTreeClient> request) {
  ViewTreeConnection::Create(this, request.Pass());
}

}  // namespace mojo
