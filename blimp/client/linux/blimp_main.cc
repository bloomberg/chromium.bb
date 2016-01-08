// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/Xlib.h>

#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "blimp/client/blimp_startup.h"
#include "blimp/client/session/blimp_client_session_linux.h"
#include "blimp/client/session/navigation_feature.h"
#include "blimp/client/session/tab_control_feature.h"

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

  blimp::client::BlimpClientSessionLinux session;
  session.GetTabControlFeature()->CreateTab(kDummyTabId);

  // If there is a non-switch argument to the command line, load that url.
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  std::string url = args.size() > 0 ? args[0] : kDefaultUrl;
  session.GetNavigationFeature()->NavigateToUrlText(kDummyTabId, url);

  base::RunLoop().Run();
}
