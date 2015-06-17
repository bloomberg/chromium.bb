// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/tests/view_manager_test_base.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_init.h"
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
    : most_recent_view_manager_(nullptr),
      window_manager_(nullptr),
      view_manager_destroyed_(false) {
}

ViewManagerTestBase::~ViewManagerTestBase() {
}

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

  view_manager_init_.reset(
      new ViewManagerInit(application_impl(), this, nullptr));
  ASSERT_TRUE(DoRunLoopWithTimeout());  // RunLoop should be quit by OnEmbed().
  std::swap(window_manager_, most_recent_view_manager_);
}

void ViewManagerTestBase::TearDown() {
  view_manager_init_.reset();  // Uses application_impl() from base class.
  ApplicationTestBase::TearDown();
}

ApplicationDelegate* ViewManagerTestBase::GetApplicationDelegate() {
  return this;
}

void ViewManagerTestBase::Initialize(ApplicationImpl* app) {
  view_manager_client_factory_.reset(
      new ViewManagerClientFactory(app->shell(), this));
}

bool ViewManagerTestBase::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(view_manager_client_factory_.get());
  return true;
}

void ViewManagerTestBase::OnEmbed(View* root) {
  most_recent_view_manager_ = root->view_manager();
  EXPECT_TRUE(QuitRunLoop());
}

void ViewManagerTestBase::OnViewManagerDestroyed(ViewManager* view_manager) {
  view_manager_destroyed_ = true;
}

}  // namespace mojo
