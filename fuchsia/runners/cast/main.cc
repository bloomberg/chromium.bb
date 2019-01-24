// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/component_context.h"
#include "base/fuchsia/service_directory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "fuchsia/runners/cast/cast_runner.h"

int main(int argc, char** argv) {
  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  CastRunner runner(
      base::fuchsia::ServiceDirectory::GetDefault(),
      WebContentRunner::CreateDefaultWebContext(),
      base::fuchsia::ComponentContext::GetDefault()
          ->ConnectToService<chromium::cast::ApplicationConfigManager>(),
      run_loop.QuitClosure());

  // Run until there are no Components, or the last service client channel is
  // closed.
  run_loop.Run();

  return 0;
}
