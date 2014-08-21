// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_runner_chromium.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"

namespace mojo {

// static
void ApplicationImpl::Terminate() {
  if (base::MessageLoop::current()->is_running())
    base::MessageLoop::current()->Quit();
}

ApplicationRunnerChromium::ApplicationRunnerChromium(
    ApplicationDelegate* delegate)
    : delegate_(scoped_ptr<ApplicationDelegate>(delegate)),
      message_loop_type_(base::MessageLoop::TYPE_DEFAULT) {}

ApplicationRunnerChromium::~ApplicationRunnerChromium() {}

void ApplicationRunnerChromium::set_message_loop_type(
    base::MessageLoop::Type type) {
  DCHECK_EQ(base::MessageLoop::TYPE_DEFAULT, message_loop_type_);
  message_loop_type_ = type;
}

MojoResult ApplicationRunnerChromium::Run(MojoHandle shell_handle) {
  base::CommandLine::Init(0, NULL);
#if !defined(COMPONENT_BUILD)
  base::AtExitManager at_exit;
#endif

  {
    base::MessageLoop loop(message_loop_type_);
    ApplicationImpl impl(delegate_.get(),
                         MakeScopedHandle(MessagePipeHandle(shell_handle)));
    loop.Run();
  }
  delegate_.reset();
  return MOJO_RESULT_OK;
}

}  // namespace mojo
