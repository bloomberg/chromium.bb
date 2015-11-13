// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/tracing/tracing_app.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new tracing::TracingApp);
  return runner.Run(shell_handle);
}
