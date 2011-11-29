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
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/webdriver/commands/alert_commands.h"
#include "chrome/test/webdriver/commands/appcache_status_command.h"
#include "chrome/test/webdriver/commands/browser_connection_commands.h"
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
#include "chrome/test/webdriver/webdriver_dispatch.h"
#include "chrome/test/webdriver/webdriver_logging.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"
#include "chrome/test/webdriver/webdriver_util.h"
#include "third_party/mongoose/mongoose.h"

#if defined(OS_WIN)
#include <time.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace webdriver {

void InitCallbacks(Dispatcher* dispatcher,
                   base::WaitableEvent* shutdown_event,
                   bool forbid_other_requests) {
  dispatcher->AddShutdown("/shutdown", shutdown_event);
  dispatcher->AddHealthz("/status");
  dispatcher->AddLog("/log");

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

  dispatcher->Add<BrowserConnectionCommand>("/session/*/browser_connection");
  dispatcher->Add<AppCacheStatusCommand>("/session/*/application_cache/status");

  // Since the /session/* is a wild card that would match the above URIs, this
  // line MUST be after all other webdriver command callbacks.
  dispatcher->Add<SessionWithID>("/session/*");

  if (forbid_other_requests)
    dispatcher->ForbidAllOtherRequests();
}

}  // namespace webdriver

namespace {

void* ProcessHttpRequest(mg_event event_raised,
                         struct mg_connection* connection,
                         const struct mg_request_info* request_info) {
  bool handler_result_code = false;
  if (event_raised == MG_NEW_REQUEST) {
    handler_result_code =
        reinterpret_cast<webdriver::Dispatcher*>(request_info->user_data)->
            ProcessHttpRequest(connection, request_info);
  }

  return reinterpret_cast<void*>(handler_result_code);
}

void MakeMongooseOptions(const std::string& port,
                         const std::string& root,
                         int http_threads,
                         bool enable_keep_alive,
                         std::vector<std::string>* out_options) {
  out_options->push_back("listening_ports");
  out_options->push_back(port);
  out_options->push_back("enable_keep_alive");
  out_options->push_back(enable_keep_alive ? "yes" : "no");
  out_options->push_back("num_threads");
  out_options->push_back(base::IntToString(http_threads));
  if (!root.empty()) {
    out_options->push_back("document_root");
    out_options->push_back(root);
  }
}

}  // namespace

// Sets up and runs the Mongoose HTTP server for the JSON over HTTP
// protcol of webdriver.  The spec is located at:
// http://code.google.com/p/selenium/wiki/JsonWireProtocol.
int main(int argc, char *argv[]) {
  base::AtExitManager exit;
  base::WaitableEvent shutdown_event(false, false);
  CommandLine::Init(argc, argv);
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();

#if defined(OS_POSIX)
  signal(SIGPIPE, SIG_IGN);
#endif
  srand((unsigned int)time(NULL));

  // Register Chrome's path provider so that the AutomationProxy will find our
  // built Chrome.
  chrome::RegisterPathProvider();
  TestTimeouts::Initialize();

  // Parse command line flags.
  std::string port = "9515";
  std::string root;
  std::string url_base;
  bool verbose = false;
  int http_threads = 4;
  bool enable_keep_alive = true;
  if (cmd_line->HasSwitch("port"))
    port = cmd_line->GetSwitchValueASCII("port");
  // The 'root' flag allows the user to specify a location to serve files from.
  // If it is not given, a callback will be registered to forbid all file
  // requests.
  if (cmd_line->HasSwitch("root"))
    root = cmd_line->GetSwitchValueASCII("root");
  if (cmd_line->HasSwitch("url-base"))
    url_base = cmd_line->GetSwitchValueASCII("url-base");
  // Whether or not to do verbose logging.
  if (cmd_line->HasSwitch("verbose"))
    verbose = true;
  if (cmd_line->HasSwitch("http-threads")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("http-threads"),
                           &http_threads)) {
      std::cerr << "'http-threads' option must be an integer";
      return 1;
    }
  }
  if (cmd_line->HasSwitch("disable-keep-alive"))
    enable_keep_alive = false;

  webdriver::InitWebDriverLogging(
      verbose ? logging::LOG_INFO : logging::LOG_WARNING);

  webdriver::SessionManager* manager = webdriver::SessionManager::GetInstance();
  manager->set_port(port);
  manager->set_url_base(url_base);

  webdriver::Dispatcher dispatcher(url_base);
  webdriver::InitCallbacks(&dispatcher, &shutdown_event, root.empty());

  std::vector<std::string> args;
  MakeMongooseOptions(port, root, http_threads, enable_keep_alive, &args);
  scoped_array<const char*> options(new const char*[args.size() + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    options[i] = args[i].c_str();
  }
  options[args.size()] = NULL;

  // Initialize SHTTPD context.
  // Listen on port 9515 or port specified on command line.
  // TODO(jmikhail) Maybe add port 9516 as a secure connection.
  struct mg_context* ctx = mg_start(&ProcessHttpRequest,
                                    &dispatcher,
                                    options.get());
  if (ctx == NULL) {
#if defined(OS_WIN)
    return WSAEADDRINUSE;
#else
    return EADDRINUSE;
#endif
  }

  // The tests depend on parsing the first line ChromeDriver outputs,
  // so all other logging should happen after this.
  std::cout << "Started ChromeDriver" << std::endl
            << "port=" << port << std::endl
            << "version=" << chrome::kChromeVersion << std::endl;

  // Run until we receive command to shutdown.
  // Don't call mg_stop because mongoose will hang if clients are still
  // connected when keep-alive is enabled.
  shutdown_event.Wait();

  return (EXIT_SUCCESS);
}
