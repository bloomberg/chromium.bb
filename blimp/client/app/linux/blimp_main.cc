// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/Xlib.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "blimp/client/app/blimp_startup.h"
#include "blimp/client/app/linux/blimp_client_session_linux.h"
#include "blimp/client/feature/navigation_feature.h"
#include "blimp/client/feature/tab_control_feature.h"
#include "blimp/client/session/assignment_source.h"

namespace {
const char kDefaultUrl[] = "https://www.google.com";
const int kDummyTabId = 0;
}

int main(int argc, const char**argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);

  XInitThreads();

  blimp::client::InitializeLogging();
  blimp::client::InitializeMainMessageLoop();

  scoped_ptr<blimp::client::AssignmentSource> assignment_source =
      make_scoped_ptr(new blimp::client::AssignmentSource(
          base::ThreadTaskRunnerHandle::Get()));
  blimp::client::BlimpClientSessionLinux session(std::move(assignment_source));
  session.GetTabControlFeature()->CreateTab(kDummyTabId);
  session.Connect();

  // If there is a non-switch argument to the command line, load that url.
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  std::string url = args.size() > 0 ? args[0] : kDefaultUrl;
  session.GetNavigationFeature()->NavigateToUrlText(kDummyTabId, url);

  base::RunLoop().Run();
}
