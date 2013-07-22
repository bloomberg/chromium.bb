// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_
#define CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/command.h"
#include "chrome/test/chromedriver/element_commands.h"
#include "chrome/test/chromedriver/net/sync_websocket_factory.h"
#include "chrome/test/chromedriver/session_commands.h"
#include "chrome/test/chromedriver/session_map.h"
#include "chrome/test/chromedriver/window_commands.h"

namespace base {
class DictionaryValue;
}

namespace net {
class HttpServerRequestInfo;
}

class Adb;
class DeviceManager;
class Log;
class HttpResponse;
class URLRequestContextGetter;

enum HttpMethod {
  kGet,
  kPost,
  kDelete,
};

struct CommandMapping {
  CommandMapping(HttpMethod method,
                 const std::string& path_pattern,
                 const Command& command);
  ~CommandMapping();

  HttpMethod method;
  std::string path_pattern;
  Command command;
};

class HttpHandler {
 public:
  HttpHandler(Log* log, const std::string& url_base);
  ~HttpHandler();

  void Handle(const net::HttpServerRequestInfo& request,
              HttpResponse* response);
  bool ShouldShutdown(const net::HttpServerRequestInfo& request);

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleUnknownCommand);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleNewSession);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleInvalidPost);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleUnimplementedCommand);
  FRIEND_TEST_ALL_PREFIXES(HttpHandlerTest, HandleCommand);
  typedef std::vector<CommandMapping> CommandMap;

  Command WrapToCommand(const SessionCommand& session_command);
  Command WrapToCommand(const WindowCommand& window_command);
  Command WrapToCommand(const ElementCommand& element_command);
  void HandleInternal(const net::HttpServerRequestInfo& request,
                      HttpResponse* response);
  bool HandleWebDriverCommand(
      const net::HttpServerRequestInfo& request,
      const std::string& trimmed_path,
      HttpResponse* response);

  Log* log_;
  base::Thread io_thread_;
  std::string url_base_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  SyncWebSocketFactory socket_factory_;
  SessionMap session_map_;
  scoped_ptr<CommandMap> command_map_;
  scoped_ptr<Adb> adb_;
  scoped_ptr<DeviceManager> device_manager_;

  DISALLOW_COPY_AND_ASSIGN(HttpHandler);
};

namespace internal {

extern const char kNewSessionPathPattern[];

bool MatchesCommand(const std::string& method,
                    const std::string& path,
                    const CommandMapping& command,
                    std::string* session_id,
                    base::DictionaryValue* out_params);

}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_
