// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/chromedriver/command_executor_impl.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "chrome/test/chromedriver/server/http_response.h"
#include "third_party/mongoose/mongoose.h"

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
  printf("Handling request: %s %s\n", request_info->uri, body.c_str());

  HttpResponse response;
  user_data->handler->Handle(request, &response);
  printf("Done handling request: %d %s\n", response.status(),
         response.body().c_str());

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

  scoped_ptr<CommandExecutor> executor(new CommandExecutorImpl());
  HttpHandler handler(executor.Pass(), HttpHandler::CreateCommandMap(),
                      url_base);
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

  if (!cmd_line->HasSwitch("silent"))
    printf("ChromeDriver (port=%s)\n", port.c_str());

  // Run until we receive command to shutdown.
  shutdown_event.Wait();

  return 0;
}
