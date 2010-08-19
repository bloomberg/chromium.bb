// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#else
#include <time.h>
#endif
#include <iostream>
#include <fstream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

#include "chrome/test/webdriver/dispatch.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/commands/create_session.h"
#include "chrome/test/webdriver/commands/session_with_id.h"

#include "third_party/mongoose/mongoose.h"

// Make sure we have ho zombies from CGIs
static void
signal_handler(int sig_num) {
  switch (sig_num) {
#ifdef OS_POSIX
  case SIGCHLD:
    while (waitpid(-1, &sig_num, WNOHANG) > 0) { }
    break;
#elif OS_WIN
  case 0:  // The win compiler demands at least 1 case statement
#endif
  default:
    break;
  }
}

namespace webdriver {
template <typename CommandType>
void SetCallback(struct mg_context* ctx, const char* pattern) {
  mg_set_uri_callback(ctx, pattern, &Dispatch<CommandType>, NULL);
}

void InitCallbacks(struct mg_context* ctx) {
  SetCallback<CreateSession>(ctx, "/session");

  // Since the /session/* is a wild card that would match the above URIs, this
  // line MUST be the last registered URI with the server
  SetCallback<SessionWithID>(ctx, "/session/*");
}
}  // namespace webdriver

// Sets up and runs the Mongoose HTTP server for the JSON over HTTP
// protcol of webdriver.  The spec is located at:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol
int main(int argc, char *argv[]) {
  struct mg_context *ctx;
  base::AtExitManager exit;
  std::string port = "8080";
#ifdef OS_POSIX
  CommandLine cmd_line = CommandLine(argc, argv);
#elif OS_WIN
  std::string c;
  for (int i = 0; i < argc; ++i) {
    c += std::string(argv[i]);
  }
  CommandLine& cmd_line = CommandLine::FromString(ASCIIToWide(c));
#endif

#if OS_POSIX
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, &signal_handler);
#endif
  srand((unsigned int)time(NULL));

  if (cmd_line.HasSwitch(std::string("port"))) {
    port = cmd_line.GetSwitchValueASCII(std::string("port"));
  }

  LOG(INFO) << "Using port: " << port;
  webdriver::SessionManager* session =
      Singleton<webdriver::SessionManager>::get();
  session->SetIPAddress(port);

  // Initialize SHTTPD context.
  // Listen on port 8080 or port specified on command line.
  // TODO(jmikhail) Maybe add port 8081 as a secure connection
  ctx = mg_start();
  mg_set_option(ctx, "ports", port.c_str());

  webdriver::InitCallbacks(ctx);

  std::cout << "Starting server" << std::endl;
  // the default behavior is to run this service forever
  while (true) {
#ifdef OS_POSIX
    sleep(3600);
#elif OS_WIN
    Sleep(3600);
#endif
  }

  // we should not reach here since the service should never quit
  // TODO(jmikhail): register a listener for SIGTERM and break the
  // message loop gracefully.
  mg_stop(ctx);
  return (EXIT_SUCCESS);
}

