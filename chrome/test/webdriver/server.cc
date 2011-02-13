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
#include "base/string_number_conversions.h"
#include "base/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/webdriver/dispatch.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/commands/create_session.h"
#include "chrome/test/webdriver/commands/execute_command.h"
#include "chrome/test/webdriver/commands/find_element_commands.h"
#include "chrome/test/webdriver/commands/implicit_wait_command.h"
#include "chrome/test/webdriver/commands/navigate_commands.h"
#include "chrome/test/webdriver/commands/session_with_id.h"
#include "chrome/test/webdriver/commands/source_command.h"
#include "chrome/test/webdriver/commands/speed_command.h"
#include "chrome/test/webdriver/commands/title_command.h"
#include "chrome/test/webdriver/commands/url_command.h"
#include "chrome/test/webdriver/commands/webelement_commands.h"
#include "third_party/mongoose/mongoose.h"

// Make sure we have ho zombies from CGIs.
static void
signal_handler(int sig_num) {
  switch (sig_num) {
#ifdef OS_POSIX
  case SIGCHLD:
    while (waitpid(-1, &sig_num, WNOHANG) > 0) { }
    break;
#elif OS_WIN
  case 0:  // The win compiler demands at least 1 case statement.
#endif
  default:
    break;
  }
}

namespace webdriver {

void Shutdown(struct mg_connection* connection,
              const struct mg_request_info* request_info,
              void* user_data) {
  base::WaitableEvent* shutdown_event =
      reinterpret_cast<base::WaitableEvent*>(user_data);
  mg_printf(connection, "HTTP/1.1 200 OK\r\n\r\n");
  shutdown_event->Signal();
}

template <typename CommandType>
void SetCallback(struct mg_context* ctx, const char* pattern) {
  mg_set_uri_callback(ctx, pattern, &Dispatch<CommandType>, NULL);
}

void InitCallbacks(struct mg_context* ctx,
                   base::WaitableEvent* shutdown_event) {
  mg_set_uri_callback(ctx, "/shutdown", &Shutdown, shutdown_event);

  SetCallback<CreateSession>(ctx,       "/session");
  SetCallback<BackCommand>(ctx,         "/session/*/back");
  SetCallback<ExecuteCommand>(ctx,      "/session/*/execute");
  SetCallback<ForwardCommand>(ctx,      "/session/*/forward");
  SetCallback<RefreshCommand>(ctx,      "/session/*/refresh");
  SetCallback<SourceCommand>(ctx,       "/session/*/source");
  SetCallback<TitleCommand>(ctx,        "/session/*/title");
  SetCallback<URLCommand>(ctx,          "/session/*/url");
  SetCallback<SpeedCommand>(ctx,        "/session/*/speed");
  SetCallback<ImplicitWaitCommand>(ctx, "/session/*/timeouts/implicit_wait");

  // WebElement commands
  SetCallback<FindOneElementCommand>(ctx,   "/session/*/element");
  SetCallback<FindManyElementsCommand>(ctx, "/session/*/elements");
  SetCallback<FindOneElementCommand>(ctx,   "/session/*/element/*/element");
  SetCallback<FindManyElementsCommand>(ctx, "/session/*/elements/*/elements");
  SetCallback<ElementValueCommand>(ctx,     "/session/*/element/*/value");
  SetCallback<ElementTextCommand>(ctx,      "/session/*/element/*/text");

  // Since the /session/* is a wild card that would match the above URIs, this
  // line MUST be the last registered URI with the server.
  SetCallback<SessionWithID>(ctx, "/session/*");
}

}  // namespace webdriver

// Configures mongoose according to the given command line flags.
// Returns true on success.
bool SetMongooseOptions(struct mg_context* ctx,
                        const std::string& port,
                        const std::string& root) {
  if (!mg_set_option(ctx, "ports", port.c_str())) {
    std::cout << "ChromeDriver cannot bind to port ("
              << port.c_str() << ")" << std::endl;
    return false;
  }
  if (root.length())
    mg_set_option(ctx, "root", root.c_str());
  // Lower the default idle time to 1 second. Idle time refers to how long a
  // worker thread will wait for new connections before exiting.
  // This is so mongoose quits in a reasonable amount of time.
  mg_set_option(ctx, "idle_time", "1");
  return true;
}

// Sets up and runs the Mongoose HTTP server for the JSON over HTTP
// protcol of webdriver.  The spec is located at:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol.
int main(int argc, char *argv[]) {
  struct mg_context *ctx;
  base::AtExitManager exit;
  base::WaitableEvent shutdown_event(false, false);
  CommandLine::Init(argc, argv);
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();

#if OS_POSIX
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, &signal_handler);
#endif
  srand((unsigned int)time(NULL));

  // Register Chrome's path provider so that the AutomationProxy will find our
  // built Chrome.
  chrome::RegisterPathProvider();
  TestTimeouts::Initialize();

  // Parse command line flags.
  std::string port = "9515";
  std::string root;
  if (cmd_line->HasSwitch("port"))
    port = cmd_line->GetSwitchValueASCII("port");
  // By default, mongoose serves files from the current working directory. The
  // 'root' flag allows the user to specify a different location to serve from.
  if (cmd_line->HasSwitch("root"))
    root = cmd_line->GetSwitchValueASCII("root");

  VLOG(1) << "Using port: " << port;
  webdriver::SessionManager* session = webdriver::SessionManager::GetInstance();
  session->SetIPAddress(port);

  // Initialize SHTTPD context.
  // Listen on port 9515 or port specified on command line.
  // TODO(jmikhail) Maybe add port 9516 as a secure connection.
  ctx = mg_start();
  if (!SetMongooseOptions(ctx, port, root)) {
    mg_stop(ctx);
    return 1;
  }

  webdriver::InitCallbacks(ctx, &shutdown_event);

  // The tests depend on parsing the first line ChromeDriver outputs,
  // so all other logging should happen after this.
  std::cout << "Started ChromeDriver" << std::endl
            << "port=" << port << std::endl;

  if (root.length()) {
    VLOG(1) << "Serving files from the current working directory";
  }

  // Run until we receive command to shutdown.
  shutdown_event.Wait();

  // We should not reach here since the service should never quit.
  // TODO(jmikhail): register a listener for SIGTERM and break the
  // message loop gracefully.
  mg_stop(ctx);
  return (EXIT_SUCCESS);
}
