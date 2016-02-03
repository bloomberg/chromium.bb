// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/test_runner/launcher.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/shell/standalone/context.h"
#include "url/gurl.h"

namespace web_view {

int LaunchTestRunner(int argc, char** argv) {
  // We want the runner::Context to outlive the MessageLoop so that pipes are
  // all gracefully closed / error-out before we try to shut the Context down.
  mojo::shell::Context shell_context;
  {
    base::MessageLoop message_loop;
    base::FilePath shell_dir;
    PathService::Get(base::DIR_MODULE, &shell_dir);
    shell_context.Init(shell_dir);

    message_loop.PostTask(FROM_HERE,
                          base::Bind(&mojo::shell::Context::Run,
                                     base::Unretained(&shell_context),
                                     GURL("mojo:web_view_test_runner")));
    message_loop.Run();

    // Must be called before |message_loop| is destroyed.
    shell_context.Shutdown();
  }

  return 0;
}

}  // namespace web_view
