// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/version.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "chrome/test/chromedriver/server/http_response.h"
#include "third_party/mongoose/mongoose.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

void ReadRequestBody(const struct mg_request_info* const request_info,
                     struct mg_connection* const connection,
                     std::string* request_body) {
  int content_length = 0;
  // 64 maximum header count hard-coded in mongoose.h
  for (int header_index = 0; header_index < 64; ++header_index) {
    if (request_info->http_headers[header_index].name == NULL) {
      break;
    }
    if (strcmp(request_info->http_headers[header_index].name,
               "Content-Length") == 0) {
      base::StringToInt(
          request_info->http_headers[header_index].value, &content_length);
      break;
    }
  }
  if (content_length > 0) {
    request_body->resize(content_length);
    int bytes_read = 0;
    while (bytes_read < content_length) {
      bytes_read += mg_read(connection,
                            &(*request_body)[bytes_read],
                            content_length - bytes_read);
    }
  }
}

struct MongooseUserData {
  HttpHandler* handler;
  base::WaitableEvent* shutdown_event;
};

void* ProcessHttpRequest(mg_event event_raised,
                         struct mg_connection* connection,
                         const struct mg_request_info* request_info) {
  if (event_raised != MG_NEW_REQUEST)
    return reinterpret_cast<void*>(false);
  MongooseUserData* user_data =
      reinterpret_cast<MongooseUserData*>(request_info->user_data);

  std::string method_str = request_info->request_method;
  HttpMethod method = kGet;
  if (method_str == "PUT" || method_str == "POST")
    method = kPost;
  else if (method_str == "DELETE")
    method = kDelete;
  std::string body;
  if (method == kPost)
    ReadRequestBody(request_info, connection, &body);

  HttpRequest request(method, request_info->uri, body);
  HttpResponse response;
  user_data->handler->Handle(request, &response);

  // Don't allow HTTP keep alive.
  response.AddHeader("connection", "close");
  std::string data;
  response.GetData(&data);
  mg_write(connection, data.data(), data.length());
  if (user_data->handler->ShouldShutdown(request))
    user_data->shutdown_event->Signal();
  return reinterpret_cast<void*>(true);
}

void MakeMongooseOptions(const std::string& port,
                         int http_threads,
                         std::vector<std::string>* out_options) {
  out_options->push_back("listening_ports");
  out_options->push_back(port);
  out_options->push_back("enable_keep_alive");
  out_options->push_back("no");
  out_options->push_back("num_threads");
  out_options->push_back(base::IntToString(http_threads));
}

}  // namespace

int main(int argc, char *argv[]) {
  CommandLine::Init(argc, argv);

  base::AtExitManager exit;
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();

  // Parse command line flags.
  std::string port = "9515";
  std::string url_base;
  int http_threads = 4;
  base::FilePath log_path;
  if (cmd_line->HasSwitch("port"))
    port = cmd_line->GetSwitchValueASCII("port");
  if (cmd_line->HasSwitch("url-base"))
    url_base = cmd_line->GetSwitchValueASCII("url-base");
  if (url_base.empty() || url_base[0] != '/')
    url_base = "/" + url_base;
  if (url_base[url_base.length() - 1] != '/')
    url_base = url_base + "/";
  if (cmd_line->HasSwitch("http-threads")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("http-threads"),
                           &http_threads)) {
      printf("'http-threads' option must be an integer\n");
      return 1;
    }
  }
  if (cmd_line->HasSwitch("log-path")) {
    log_path = cmd_line->GetSwitchValuePath("log-path");
#if defined(OS_WIN)
    FILE* redir_stderr = _wfreopen(log_path.value().c_str(), L"w", stderr);
#else
    FILE* redir_stderr = freopen(log_path.value().c_str(), "w", stderr);
#endif
    if (!redir_stderr) {
      printf("Failed to redirect stderr to log file. Exiting...\n");
      return 1;
    }
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  bool success = logging::InitLogging(settings);
  if (!success) {
    PLOG(ERROR) << "Unable to initialize logging";
  }
  logging::SetLogItems(false,  // enable_process_id
                       false,  // enable_thread_id
                       false,  // enable_timestamp
                       false); // enable_tickcount
  Log::Level level = Log::kLog;
  if (cmd_line->HasSwitch("verbose"))
    level = Log::kDebug;

  scoped_ptr<Log> log(new Logger(level));
  scoped_ptr<CommandExecutor> executor(new CommandExecutorImpl(log.get()));
  HttpHandler handler(
      log.get(), executor.Pass(), HttpHandler::CreateCommandMap(), url_base);
  base::WaitableEvent shutdown_event(false, false);
  MongooseUserData user_data = { &handler, &shutdown_event };

  std::vector<std::string> args;
  MakeMongooseOptions(port, http_threads, &args);
  scoped_ptr<const char*[]> options(new const char*[args.size() + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    options[i] = args[i].c_str();
  }
  options[args.size()] = NULL;

  struct mg_context* ctx = mg_start(&ProcessHttpRequest,
                                    &user_data,
                                    options.get());
  if (ctx == NULL) {
    printf("Port already in use. Exiting...\n");
    return 1;
  }

  if (!cmd_line->HasSwitch("silent")) {
    printf("Started ChromeDriver (v%s) on port %s\n",
           kChromeDriverVersion,
           port.c_str());
  }

#if defined(OS_POSIX)
  if (!cmd_line->HasSwitch("verbose")) {
    // Close stderr on exec, so that Chrome log spew doesn't confuse users.
    fcntl(STDERR_FILENO, F_SETFD, FD_CLOEXEC);
  }
#endif

  // Run until we receive command to shutdown.
  shutdown_event.Wait();

  return 0;
}
