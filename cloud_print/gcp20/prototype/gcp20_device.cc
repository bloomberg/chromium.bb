// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cloud_print/gcp20/prototype/printer.h"

namespace {

void StartPrinter(Printer* printer) {
  bool success = printer->Start();
  DCHECK(success);
}

base::RunLoop* g_runner = NULL;

void OnAbort(int val) {
  if (g_runner)
    g_runner->Quit();
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit;
  Printer printer;
  CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  signal(SIGINT, OnAbort);  // Handle Ctrl+C signal.

  base::MessageLoop loop(base::MessageLoop::TYPE_IO);
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(&StartPrinter, &printer));
  base::RunLoop runner;
  g_runner = &runner;
  runner.Run();
  g_runner = NULL;
  printer.Stop();

  return 0;
}

