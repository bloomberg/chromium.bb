// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/app_child_process.h"

#include "base/logging.h"

namespace mojo {
namespace shell {

AppChildProcess::AppChildProcess() {
}

AppChildProcess::~AppChildProcess() {
}

void AppChildProcess::Main() {
  VLOG(2) << "AppChildProcess::Main()";

  // TODO(vtl)

  platform_channel()->reset();
}

}  // namespace shell
}  // namespace mojo
