// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <locale>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/net/port_server.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "chrome/test/chromedriver/version.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_server_socket.h"

namespace {

const char kLocalHostAddress[] = "127.0.0.1";
const int kBufferSize = 100 * 1024 * 1024;  // 100 MB

typedef base::Callback<
    void(const net::HttpServerRequestInfo&, const HttpResponseSenderFunc&)>
    HttpRequestHandlerFunc;

class HttpServer : public net::HttpServer::Delegate {
 public:
  explicit HttpServer(const HttpRequestHandlerFunc& handle_request_func)
      : handle_request_func_(handle_request_func),
        weak_factory_(this) {}

  ~HttpServer() override {}

  bool Start(uint16_t port, bool allow_remote) {
    std::string binding_ip = kLocalHostAddress;
    if (allow_remote)
      binding_ip = "0.0.0.0";
    scoped_ptr<net::ServerSocket> server_socket(
        new net::TCPServerSocket(NULL, net::NetLog::Source()));
    server_socket->ListenWithAddressAndPort(binding_ip, port, 1);
    server_.reset(new net::HttpServer(std::move(server_socket), this));
    net::IPEndPoint address;
    return server_->GetLocalAddress(&address) == net::OK;
  }

  // Overridden from net::HttpServer::Delegate:
  void OnConnect(int connection_id) override {
    server_->SetSendBufferSize(connection_id, kBufferSize);
    server_->SetReceiveBufferSize(connection_id, kBufferSize);
  }
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override {
    handle_request_func_.Run(
        info,
        base::Bind(&HttpServer::OnResponse,
                   weak_factory_.GetWeakPtr(),
                   connection_id));
  }
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override {}
  void OnWebSocketMessage(int connection_id, const std::string& data) override {
  }
  void OnClose(int connection_id) override {}

 private:
  void OnResponse(int connection_id,
                  scoped_ptr<net::HttpServerResponseInfo> response) {
    // Don't support keep-alive, since there's no way to detect if the
    // client is HTTP/1.0. In such cases, the client may hang waiting for
    // the connection to close (e.g., python 2.7 urllib).
    response->AddHeader("Connection", "close");
    server_->SendResponse(connection_id, *response);
    // Don't need to call server_->Close(), since SendResponse() will handle
    // this for us.
  }

  HttpRequestHandlerFunc handle_request_func_;
  scoped_ptr<net::HttpServer> server_;
  base::WeakPtrFactory<HttpServer> weak_factory_;  // Should be last.
};

void SendResponseOnCmdThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const HttpResponseSenderFunc& send_response_on_io_func,
    scoped_ptr<net::HttpServerResponseInfo> response) {
  io_task_runner->PostTask(
      FROM_HERE, base::Bind(send_response_on_io_func, base::Passed(&response)));
}

void HandleRequestOnCmdThread(
    HttpHandler* handler,
    const std::vector<std::string>& whitelisted_ips,
    const net::HttpServerRequestInfo& request,
    const HttpResponseSenderFunc& send_response_func) {
  if (!whitelisted_ips.empty()) {
    std::string peer_address = request.peer.ToStringWithoutPort();
    if (peer_address != kLocalHostAddress &&
        std::find(whitelisted_ips.begin(), whitelisted_ips.end(),
                  peer_address) == whitelisted_ips.end()) {
      LOG(WARNING) << "unauthorized access from " << request.peer.ToString();
      scoped_ptr<net::HttpServerResponseInfo> response(
          new net::HttpServerResponseInfo(net::HTTP_UNAUTHORIZED));
      response->SetBody("Unauthorized access", "text/plain");
      send_response_func.Run(std::move(response));
      return;
    }
  }

  handler->Handle(request, send_response_func);
}

void HandleRequestOnIOThread(
    const scoped_refptr<base::SingleThreadTaskRunner>& cmd_task_runner,
    const HttpRequestHandlerFunc& handle_request_on_cmd_func,
    const net::HttpServerRequestInfo& request,
    const HttpResponseSenderFunc& send_response_func) {
  cmd_task_runner->PostTask(
      FROM_HERE, base::Bind(handle_request_on_cmd_func, request,
                            base::Bind(&SendResponseOnCmdThread,
                                       base::ThreadTaskRunnerHandle::Get(),
                                       send_response_func)));
}

base::LazyInstance<base::ThreadLocalPointer<HttpServer> >
    lazy_tls_server = LAZY_INSTANCE_INITIALIZER;

void StopServerOnIOThread() {
  // Note, |server| may be NULL.
  HttpServer* server = lazy_tls_server.Pointer()->Get();
  lazy_tls_server.Pointer()->Set(NULL);
  delete server;
}

void StartServerOnIOThread(uint16_t port,
                           bool allow_remote,
                           const HttpRequestHandlerFunc& handle_request_func) {
  scoped_ptr<HttpServer> temp_server(new HttpServer(handle_request_func));
  if (!temp_server->Start(port, allow_remote)) {
    printf("Port not available. Exiting...\n");
    exit(1);
  }
  lazy_tls_server.Pointer()->Set(temp_server.release());
}

void RunServer(uint16_t port,
               bool allow_remote,
               const std::vector<std::string>& whitelisted_ips,
               const std::string& url_base,
               int adb_port,
               scoped_ptr<PortServer> port_server) {
  base::Thread io_thread("ChromeDriver IO");
  CHECK(io_thread.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));

  base::MessageLoop cmd_loop;
  base::RunLoop cmd_run_loop;
  HttpHandler handler(cmd_run_loop.QuitClosure(), io_thread.task_runner(),
                      url_base, adb_port, std::move(port_server));
  HttpRequestHandlerFunc handle_request_func =
      base::Bind(&HandleRequestOnCmdThread, &handler, whitelisted_ips);

  io_thread.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&StartServerOnIOThread, port, allow_remote,
                 base::Bind(&HandleRequestOnIOThread, cmd_loop.task_runner(),
                            handle_request_func)));
  // Run the command loop. This loop is quit after the response for a shutdown
  // request is posted to the IO loop. After the command loop quits, a task
  // is posted to the IO loop to stop the server. Lastly, the IO thread is
  // destroyed, which waits until all pending tasks have been completed.
  // This assumes the response is sent synchronously as part of the IO task.
  cmd_run_loop.Run();
  io_thread.message_loop()
      ->PostTask(FROM_HERE, base::Bind(&StopServerOnIOThread));
}

}  // namespace

int main(int argc, char *argv[]) {
  base::CommandLine::Init(argc, argv);

  base::AtExitManager at_exit;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

#if defined(OS_LINUX)
  // Select the locale from the environment by passing an empty string instead
  // of the default "C" locale. This is particularly needed for the keycode
  // conversion code to work.
  setlocale(LC_ALL, "");
#endif

  // Parse command line flags.
  uint16_t port = 9515;
  int adb_port = 5037;
  bool allow_remote = false;
  std::vector<std::string> whitelisted_ips;
  std::string url_base;
  scoped_ptr<PortServer> port_server;
  if (cmd_line->HasSwitch("h") || cmd_line->HasSwitch("help")) {
    std::string options;
    const char* const kOptionAndDescriptions[] = {
        "port=PORT", "port to listen on",
        "adb-port=PORT", "adb server port",
        "log-path=FILE", "write server log to file instead of stderr, "
            "increases log level to INFO",
        "verbose", "log verbosely",
        "version", "print the version number and exit",
        "silent", "log nothing",
        "url-base", "base URL path prefix for commands, e.g. wd/url",
        "port-server", "address of server to contact for reserving a port",
        "whitelisted-ips", "comma-separated whitelist of remote IPv4 addresses "
            "which are allowed to connect to ChromeDriver",
    };
    for (size_t i = 0; i < arraysize(kOptionAndDescriptions) - 1; i += 2) {
      options += base::StringPrintf(
          "  --%-30s%s\n",
          kOptionAndDescriptions[i], kOptionAndDescriptions[i + 1]);
    }
    printf("Usage: %s [OPTIONS]\n\nOptions\n%s", argv[0], options.c_str());
    return 0;
  }
  if (cmd_line->HasSwitch("v") || cmd_line->HasSwitch("version")) {
    printf("ChromeDriver %s\n", kChromeDriverVersion);
    return 0;
  }
  if (cmd_line->HasSwitch("port")) {
    int cmd_line_port;
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("port"),
                           &cmd_line_port) ||
        cmd_line_port < 0 || cmd_line_port > 65535) {
      printf("Invalid port. Exiting...\n");
      return 1;
    }
    port = static_cast<uint16_t>(cmd_line_port);
  }
  if (cmd_line->HasSwitch("adb-port")) {
    if (!base::StringToInt(cmd_line->GetSwitchValueASCII("adb-port"),
                           &adb_port)) {
      printf("Invalid adb-port. Exiting...\n");
      return 1;
    }
  }
  if (cmd_line->HasSwitch("port-server")) {
#if defined(OS_LINUX)
    std::string address = cmd_line->GetSwitchValueASCII("port-server");
    if (address.empty() || address[0] != '@') {
      printf("Invalid port-server. Exiting...\n");
      return 1;
    }
    std::string path;
    // First character of path is \0 to use Linux's abstract namespace.
    path.push_back(0);
    path += address.substr(1);
    port_server.reset(new PortServer(path));
#else
    printf("Warning: port-server not implemented for this platform.\n");
#endif
  }
  if (cmd_line->HasSwitch("url-base"))
    url_base = cmd_line->GetSwitchValueASCII("url-base");
  if (url_base.empty() || url_base.front() != '/')
    url_base = "/" + url_base;
  if (url_base.back() != '/')
    url_base = url_base + "/";
  if (cmd_line->HasSwitch("whitelisted-ips")) {
    allow_remote = true;
    std::string whitelist = cmd_line->GetSwitchValueASCII("whitelisted-ips");
    whitelisted_ips = base::SplitString(
        whitelist, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  if (!cmd_line->HasSwitch("silent")) {
    printf("Starting ChromeDriver %s on port %u\n", kChromeDriverVersion, port);
    if (!allow_remote) {
      printf("Only local connections are allowed.\n");
    } else if (!whitelisted_ips.empty()) {
      printf("Remote connections are allowed by a whitelist (%s).\n",
             cmd_line->GetSwitchValueASCII("whitelisted-ips").c_str());
    } else {
      printf("All remote connections are allowed. Use a whitelist instead!\n");
    }
    fflush(stdout);
  }

  if (!InitLogging()) {
    printf("Unable to initialize logging. Exiting...\n");
    return 1;
  }
  RunServer(port, allow_remote, whitelisted_ips, url_base, adb_port,
            std::move(port_server));
  return 0;
}
