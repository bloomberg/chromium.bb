// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "media/mojo/services/mojo_media_application.h"
#include "mojo/logging/init_logging.h"
#include "mojo/public/c/system/main.h"
#include "mojo/shell/public/cpp/application_runner.h"

MojoResult MojoMain(MojoHandle mojo_handle) {
  // Enable logging.
  base::AtExitManager at_exit;
  mojo::ApplicationRunner::InitBaseCommandLine();
  mojo::InitLogging();

  scoped_ptr<mojo::ApplicationDelegate> app_delegate =
      media::MojoMediaApplication::CreateApp();
  mojo::ApplicationRunner runner(app_delegate.release());
  return runner.Run(mojo_handle, false /* init_base */);
}
