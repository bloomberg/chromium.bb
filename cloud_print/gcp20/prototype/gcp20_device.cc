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

const char kHelpMessage[] =
    "usage: gcp20_device [switches] [options]\n"
    "\n"
    "switches:\n"
    "  --disable-confirmation     disables confirmation of registration\n"
    "  --disable-method-check     disables HTTP method checking (POST, GET)\n"
    "  --disable-x-token          disables checking of X-Privet-Token "
                                 "HTTP header\n"
    "  -h, --help                 prints this message\n"
    "  --no-announcement          disables DNS announcements\n"
    "  --extended-response        responds to PTR with additional records\n"
    "  --simulate-printing-errors simulates some errors for local printing\n"
    "  --unicast-respond          DNS responses will be sent in unicast "
                                 "instead of multicast\n"
    "\n"
    "options:\n"
    "  --domain-name=<name>       sets, should ends with '.local'\n"
    "  --http-port=<value>        sets port for HTTP server\n"
    "  --service-name=<name>      sets DNS service name\n"
    "  --state-path=<path>        sets path to file with registration state\n"
    "  --ttl=<value>              sets TTL for DNS announcements\n"
    "\n"
    "WARNING: mDNS probing is not implemented\n";

void PrintHelp() {
  printf("%s", kHelpMessage);
}

void StartPrinter(Printer* printer) {
  bool success = printer->Start();
  DCHECK(success);
}

base::RunLoop* g_runner = NULL;
Printer* g_printer = NULL;
base::MessageLoop* g_message_loop;

void StopLoop() {
  // Always do after printer.Stop() to make sure XMPP will
  // be disabled fully before |Quit| will be called
  // (XMPP disables itself via MessageLoop call).
  g_message_loop->PostTask(FROM_HERE, g_runner->QuitClosure());
  g_message_loop = NULL;
  g_runner = NULL;
}

void OnAbort(int val) {
  if (g_printer) {
    g_message_loop->PostTask(
        FROM_HERE,
        base::Bind(&Printer::Stop, base::Unretained(g_printer)));
    g_message_loop->PostTask(FROM_HERE, base::Bind(&StopLoop));
    g_printer = NULL;
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  base::AtExitManager at_exit;
  Printer printer;
  CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  if (CommandLine::ForCurrentProcess()->HasSwitch("h") ||
      CommandLine::ForCurrentProcess()->HasSwitch("help")) {
    PrintHelp();
    return 0;
  }

  signal(SIGINT, OnAbort);  // Handle Ctrl+C signal.

  base::MessageLoopForIO loop;
  g_message_loop = &loop;
  g_message_loop->PostTask(FROM_HERE, base::Bind(&StartPrinter, &printer));
  base::RunLoop runner;
  g_printer = &printer;
  g_runner = &runner;
  runner.Run();

  return 0;
}

