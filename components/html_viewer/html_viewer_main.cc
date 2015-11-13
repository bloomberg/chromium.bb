// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_viewer.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new html_viewer::HTMLViewer);
  return runner.Run(shell_handle);
}
