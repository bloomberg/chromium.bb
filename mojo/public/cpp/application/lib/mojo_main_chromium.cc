// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::CommandLine::Init(0, NULL);
#if !defined(COMPONENT_BUILD)
  base::AtExitManager at_exit;
#endif
  base::MessageLoop loop;
  scoped_ptr<mojo::ApplicationDelegate> delegate(
      mojo::ApplicationDelegate::Create());
  mojo::ApplicationImpl app(delegate.get());
  app.BindShell(shell_handle);
  loop.Run();

  return MOJO_RESULT_OK;
}
