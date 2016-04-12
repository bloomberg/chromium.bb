// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/memory/ptr_util.h"
#include "media/mojo/services/mojo_media_application.h"
#include "media/mojo/services/test_mojo_media_client.h"
#include "mojo/logging/init_logging.h"
#include "mojo/public/c/system/main.h"
#include "services/shell/public/cpp/application_runner.h"

MojoResult MojoMain(MojoHandle mojo_handle) {
  // Enable logging.
  base::AtExitManager at_exit;
  mojo::ApplicationRunner::InitBaseCommandLine();
  mojo::InitLogging();

  mojo::ApplicationRunner runner(new media::MojoMediaApplication(
      base::WrapUnique(new media::TestMojoMediaClient())));
  return runner.Run(mojo_handle, false /* init_base */);
}
