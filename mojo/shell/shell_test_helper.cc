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
  ScopedShellHandle shell_handle;
};

namespace {

void StartShellOnShellThread(ShellTestHelper::State* state) {
  state->context.reset(new Context);
  state->test_api.reset(
      new ServiceManager::TestAPI(state->context->service_manager()));
  state->shell_handle = state->test_api->GetShellHandle();
}

}  // namespace

class ShellTestHelper::TestShellClient : public ShellClient {
 public:
  TestShellClient() {}
  virtual ~TestShellClient() {}

  // ShellClient:
  virtual void AcceptConnection(
      const mojo::String& url,
      ScopedMessagePipeHandle client_handle) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestShellClient);
};

ShellTestHelper::ShellTestHelper()
    : shell_thread_("Test Shell Thread"),
      state_(NULL) {
  CommandLine::Init(0, NULL);
  mojo::shell::InitializeLogging();
}

ShellTestHelper::~ShellTestHelper() {
  if (state_) {
    // |state_| contains data created on the background thread. Destroy it
    // there so that there aren't any race conditions.
    shell_thread_.message_loop()->DeleteSoon(FROM_HERE, state_);
    state_ = NULL;
  }
}

void ShellTestHelper::Init() {
  DCHECK(!state_);
  state_ = new State;
  shell_thread_.Start();
  shell_thread_.message_loop()->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&StartShellOnShellThread, state_),
      base::Bind(&ShellTestHelper::OnShellStarted, base::Unretained(this)));
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();
}

void ShellTestHelper::OnShellStarted() {
  DCHECK(state_);
  shell_client_.reset(new TestShellClient);
  shell_.reset(state_->shell_handle.Pass(), shell_client_.get());
  run_loop_->Quit();
}

}  // namespace shell
}  // namespace mojo
