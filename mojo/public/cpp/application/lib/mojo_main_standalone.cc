// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/utility/run_loop.h"

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  mojo::Environment env;
  mojo::RunLoop loop;
  mojo::ApplicationDelegate* delegate = mojo::ApplicationDelegate::Create();
  {
    mojo::ApplicationImpl app(delegate);
    app.BindShell(shell_handle);
    loop.Run();
  }
  delete delegate;

  return MOJO_RESULT_OK;
}
