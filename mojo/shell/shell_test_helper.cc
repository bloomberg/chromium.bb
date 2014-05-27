// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_test_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"

namespace mojo {
namespace shell {

// State used on the background thread. Be careful, this is created on the main
// thread than passed to the shell thread. Destruction happens on the shell
// thread.
struct ShellTestHelper::State {
  scoped_ptr<Context> context;
  scoped_ptr<ServiceManager::TestAPI> test_api;
  ScopedMessagePipeHandle service_provider_handle;
};

namespace {

void StartShellOnShellThread(ShellTestHelper::State* state) {
  state->context.reset(new Context);
  state->test_api.reset(
      new ServiceManager::TestAPI(state->context->service_manager()));
  state->service_provider_handle = state->test_api->GetServiceProviderHandle();
}

}  // namespace

class ShellTestHelper::TestServiceProvider : public ServiceProvider {
 public:
  TestServiceProvider() {}
  virtual ~TestServiceProvider() {}

  // ServiceProvider:
  virtual void ConnectToService(
      const mojo::String& url,
      ScopedMessagePipeHandle client_handle) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestServiceProvider);
};

ShellTestHelper::ShellTestHelper()
    : service_provider_thread_("shell_test_helper"),
      state_(NULL) {
  base::CommandLine::Init(0, NULL);
  mojo::shell::InitializeLogging();
}

ShellTestHelper::~ShellTestHelper() {
  if (state_) {
    // |state_| contains data created on the background thread. Destroy it
    // there so that there aren't any race conditions.
    service_provider_thread_.message_loop()->DeleteSoon(FROM_HERE, state_);
    state_ = NULL;
  }
}

void ShellTestHelper::Init() {
  DCHECK(!state_);
  state_ = new State;
  service_provider_thread_.Start();
  base::MessageLoopProxy* message_loop_proxy =
      service_provider_thread_.message_loop()->message_loop_proxy();
  message_loop_proxy->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&StartShellOnShellThread, state_),
      base::Bind(&ShellTestHelper::OnServiceProviderStarted,
                 base::Unretained(this)));
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();
}

void ShellTestHelper::OnServiceProviderStarted() {
  DCHECK(state_);
  local_service_provider_.reset(new TestServiceProvider);
  service_provider_.Bind(state_->service_provider_handle.Pass());
  service_provider_.set_client(local_service_provider_.get());
  run_loop_->Quit();
}

}  // namespace shell
}  // namespace mojo
