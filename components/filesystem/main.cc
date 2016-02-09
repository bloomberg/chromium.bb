// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/filesystem/file_system_app.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/application_runner.h"

MojoResult MojoMain(MojoHandle request) {
  mojo::ApplicationRunner runner(new filesystem::FileSystemApp());
  return runner.Run(request);
}
