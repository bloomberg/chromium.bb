// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>
#include <stdlib.h>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <iostream>
#include <fstream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/webdriver/dispatch.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/commands/alert_commands.h"
#include "chrome/test/webdriver/commands/cookie_commands.h"
#include "chrome/test/webdriver/commands/create_session.h"
#include "chrome/test/webdriver/commands/execute_async_script_command.h"
#include "chrome/test/webdriver/commands/execute_command.h"
#include "chrome/test/webdriver/commands/find_element_commands.h"
#include "chrome/test/webdriver/commands/navigate_commands.h"
#include "chrome/test/webdriver/commands/mouse_commands.h"
#include "chrome/test/webdriver/commands/screenshot_command.h"
#include "chrome/test/webdriver/commands/session_with_id.h"
#include "chrome/test/webdriver/commands/set_timeout_commands.h"
#include "chrome/test/webdriver/commands/source_command.h"
#include "chrome/test/webdriver/commands/target_locator_commands.h"
#include "chrome/test/webdriver/commands/title_command.h"
#include "chrome/test/webdriver/commands/url_command.h"
#include "chrome/test/webdriver/commands/webelement_commands.h"
#include "third_party/mongoose/mongoose.h"

#if defined(OS_WIN)
#include <time.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

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

void InitCallbacks(struct mg_context* ctx, Dispatcher* dispatcher,
                   base::WaitableEvent* shutdown_event,
                   bool forbid_other_requests) {
  dispatcher->AddShutdown("/shutdown", shutdown_event);
  dispatcher->AddStatus("/healthz");

  dispatcher->Add<CreateSession>("/session");

  // WebElement commands
  dispatcher->Add<FindOneElementCommand>(  "/session/*/element");
  dispatcher->Add<FindManyElementsCommand>("/session/*/elements");
  dispatcher->Add<ActiveElementCommand>(   "/session/*/element/active");
  dispatcher->Add<FindOneElementCommand>(  "/session/*/element/*/element");
  dispatcher->Add<FindManyElementsCommand>("/session/*/elements/*/elements");
  dispatcher->Add<ElementAttributeCommand>("/session/*/element/*/attribute/*");
  dispatcher->Add<ElementCssCommand>(      "/session/*/element/*/css/*");
  dispatcher->Add<ElementClearCommand>(    "/session/*/element/*/clear");
  dispatcher->Add<ElementDisplayedCommand>("/session/*/element/*/displayed");
  dispatcher->Add<ElementEnabledCommand>(  "/session/*/element/*/enabled");
  dispatcher->Add<ElementEqualsCommand>(   "/session/*/element/*/equals/*");
  dispatcher->Add<ElementLocationCommand>( "/session/*/element/*/location");
  dispatcher->Add<ElementLocationInViewCommand>(
      "/session/*/element/*/location_in_view");
  dispatcher->Add<ElementNameCommand>(    "/session/*/element/*/name");
  dispatcher->Add<ElementSelectedCommand>("/session/*/element/*/selected");
  dispatcher->Add<ElementSizeCommand>(    "/session/*/element/*/size");
  dispatcher->Add<ElementSubmitCommand>(  "/session/*/element/*/submit");
  dispatcher->Add<ElementTextCommand>(    "/session/*/element/*/text");
  dispatcher->Add<ElementToggleCommand>(  "/session/*/element/*/toggle");
  dispatcher->Add<ElementValueCommand>(   "/session/*/element/*/value");

  dispatcher->Add<ScreenshotCommand>("/session/*/screenshot");

  // Mouse Commands
  dispatcher->Add<MoveAndClickCommand>("/session/*/element/*/click");
  dispatcher->Add<DragCommand>(        "/session/*/element/*/drag");
  dispatcher->Add<HoverCommand>(       "/session/*/element/*/hover");

  dispatcher->Add<MoveToCommand>(     "/session/*/moveto");
  dispatcher->Add<ClickCommand>(      "/session/*/click");
  dispatcher->Add<ButtonDownCommand>( "/session/*/buttondown");
  dispatcher->Add<ButtonUpCommand>(   "/session/*/buttonup");
  dispatcher->Add<DoubleClickCommand>("/session/*/doubleclick");

  // All session based commands should be listed after the element based
  // commands to avoid potential mapping conflicts from an overzealous
  // wildcard match. For example, /session/*/title maps to the handler to
  // fetch the page title. If mapped first, this would overwrite the handler
  // for /session/*/element/*/attribute/title, which should fetch the title
  // attribute of the element.
  dispatcher->Add<AcceptAlertCommand>(  "/session/*/accept_alert");
  dispatcher->Add<AlertTextCommand>(    "/session/*/alert_text");
  dispatcher->Add<BackCommand>(         "/session/*/back");
  dispatcher->Add<DismissAlertCommand>( "/session/*/dismiss_alert");
  dispatcher->Add<ExecuteCommand>(      "/session/*/execute");
  dispatcher->Add<ExecuteAsyncScriptCommand>(
                                        "/session/*/execute_async");
  dispatcher->Add<ForwardCommand>(      "/session/*/forward");
  dispatcher->Add<SwitchFrameCommand>(  "/session/*/frame");
  dispatcher->Add<RefreshCommand>(      "/session/*/refresh");
  dispatcher->Add<SourceCommand>(       "/session/*/source");
  dispatcher->Add<TitleCommand>(        "/session/*/title");
  dispatcher->Add<URLCommand>(          "/session/*/url");
  dispatcher->Add<WindowCommand>(       "/session/*/window");
  dispatcher->Add<WindowHandleCommand>( "/session/*/window_handle");
  dispatcher->Add<WindowHandlesCommand>("/session/*/window_handles");
  dispatcher->Add<SetAsyncScriptTimeoutCommand>(
                                        "/session/*/timeouts/async_script");
  dispatcher->Add<ImplicitWaitCommand>( "/session/*/timeouts/implicit_wait");

  // Cookie functions.
  dispatcher->Add<CookieCommand>(     "/session/*/cookie");
  dispatcher->Add<NamedCookieCommand>("/session/*/cookie/*");

  // Since the /session/* is a wild card that would match the above URIs, this
  // line MUST be after all other webdriver command callbacks.
  dispatcher->Add<SessionWithID>("/session/*");

  if (forbid_other_requests)
    dispatcher->ForbidAllOtherRequests();
}

}  // namespace webdriver

// Initializes logging for ChromeDriver.
void InitChromeDriverLogging(const CommandLine& command_line) {
  bool success = InitLogging(
      FILE_PATH_LITERAL("chromedriver.log"),
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  if (!success) {
    PLOG(ERROR) << "Unable to initialize logging";
  }
  logging::SetLogItems(false,  // enable_process_id
                       false,  // enable_thread_id
                       true,   // enable_timestamp
                       false); // enable_tickcount
  if (command_line.HasSwitch(switches::kLoggingLevel)) {
    std::string log_level = command_line.GetSwitchValueASCII(
        switches::kLoggingLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level)) {
      logging::SetMinLogLevel(level);
    } else {
      LOG(WARNING) << "Bad log level: " << log_level;
    }
  }
}

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
  InitChromeDriverLogging(*cmd_line);

  // Parse command line flags.
  std::string port = "9515";
  std::string root;
  std::string url_base;
  if (cmd_line->HasSwitch("port"))
    port = cmd_line->GetSwitchValueASCII("port");
  // The 'root' flag allows the user to specify a location to serve files from.
  // If it is not given, a callback will be registered to forbid all file
  // requests.
  if (cmd_line->HasSwitch("root"))
    root = cmd_line->GetSwitchValueASCII("root");
  if (cmd_line->HasSwitch("url-base"))
    url_base = cmd_line->GetSwitchValueASCII("url-base");

  webdriver::SessionManager* manager = webdriver::SessionManager::GetInstance();
  manager->set_port(port);
  manager->set_url_base(url_base);

  // Initialize SHTTPD context.
  // Listen on port 9515 or port specified on command line.
  // TODO(jmikhail) Maybe add port 9516 as a secure connection.
  ctx = mg_start();
  if (!SetMongooseOptions(ctx, port, root)) {
    mg_stop(ctx);
#if defined(OS_WIN)
    return WSAEADDRINUSE;
#else
    return EADDRINUSE;
#endif
  }

  webdriver::Dispatcher dispatcher(ctx, url_base);
  webdriver::InitCallbacks(ctx, &dispatcher, &shutdown_event, root.empty());

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
