// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_
#define CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

class CommandExecutor;
class HttpResponse;

enum HttpMethod {
  kGet = 0,
  kPost,
  kDelete,
};

struct HttpRequest {
  HttpRequest(HttpMethod method,
              const std::string& path,
              const std::string& body);
  ~HttpRequest();

  HttpMethod method;
  std::string path;
  std::string body;
};

struct CommandMapping {
  CommandMapping(HttpMethod method,
                 const std::string& path_pattern,
                 const std::string& name);
  ~CommandMapping();

  HttpMethod method;
  std::string path_pattern;
  std::string name;
};

class HttpHandler {
 public:
  typedef std::vector<CommandMapping> CommandMap;
  static scoped_ptr<CommandMap> CreateCommandMap();

  HttpHandler(scoped_ptr<CommandExecutor> executor,
              scoped_ptr<std::vector<CommandMapping> > commands,
              const std::string& url_base);
  ~HttpHandler();

  void Handle(const HttpRequest& request,
              HttpResponse* response);

  bool ShouldShutdown(const HttpRequest& request);

 private:
  bool HandleWebDriverCommand(
      const HttpRequest& request,
      const std::string& trimmed_path,
      HttpResponse* response);

  scoped_ptr<CommandExecutor> executor_;
  scoped_ptr<CommandMap> command_map_;
  std::string url_base_;
};

namespace internal {
extern const char kNewSessionIdCommand[];
bool MatchesCommand(HttpMethod method,
                    const std::string& path,
                    const CommandMapping& command,
                    std::string* session_id,
                    base::DictionaryValue* out_params);
}  // namespace internal

#endif  // CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_HANDLER_H_
