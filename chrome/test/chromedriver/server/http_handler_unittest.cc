// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/command_executor.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "chrome/test/chromedriver/server/http_response.h"
#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DummyExecutor : public CommandExecutor {
 public:
  DummyExecutor() : status_(kOk) {}
  explicit DummyExecutor(StatusCode status) : status_(status) {}
  virtual ~DummyExecutor() {}

  virtual void Init() OVERRIDE {}
  virtual void ExecuteCommand(const std::string& name,
                              const base::DictionaryValue& params,
                              const std::string& session_id,
                              StatusCode* status,
                              scoped_ptr<base::Value>* value,
                              std::string* out_session_id) OVERRIDE {
    *status = status_;
    value->reset(base::Value::CreateIntegerValue(1));
    *out_session_id = "session_id";
  }

 private:
  StatusCode status_;
};

}

TEST(HttpHandlerTest, HandleOutsideOfBaseUrl) {
  HttpHandler handler(
      scoped_ptr<CommandExecutor>(new DummyExecutor()),
      scoped_ptr<HttpHandler::CommandMap>(new HttpHandler::CommandMap()),
      "base/url/");
  HttpRequest request(kGet, "base/path", "body");
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kBadRequest, response.status());
}

TEST(HttpHandlerTest, HandleUnknownCommand) {
  HttpHandler handler(
      scoped_ptr<CommandExecutor>(new DummyExecutor()),
      scoped_ptr<HttpHandler::CommandMap>(new HttpHandler::CommandMap()),
      "/");
  HttpRequest request(kGet, "/path", "");
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kNotFound, response.status());
}

TEST(HttpHandlerTest, HandleNewSession) {
  scoped_ptr<HttpHandler::CommandMap> map(new HttpHandler::CommandMap());
  map->push_back(CommandMapping(kPost, "new", internal::kNewSessionIdCommand));
  HttpHandler handler(
      scoped_ptr<CommandExecutor>(new DummyExecutor()),
      map.Pass(), "/base/");
  HttpRequest request(kPost, "/base/new", "");
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kSeeOther, response.status());
  std::string location;
  ASSERT_TRUE(response.GetHeader("Location", &location));
  std::string prefix = "/base/session/";
  ASSERT_EQ(prefix, location.substr(0, prefix.length()));
}

TEST(HttpHandlerTest, HandleInvalidPost) {
  scoped_ptr<HttpHandler::CommandMap> map(new HttpHandler::CommandMap());
  map->push_back(CommandMapping(kPost, "path", "cmd"));
  HttpHandler handler(
      scoped_ptr<CommandExecutor>(new DummyExecutor()),
      map.Pass(), "/");
  HttpRequest request(kPost, "/path", "should be a dictionary");
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kBadRequest, response.status());
}

TEST(HttpHandlerTest, HandleUnimplementedCommand) {
  scoped_ptr<HttpHandler::CommandMap> map(new HttpHandler::CommandMap());
  map->push_back(CommandMapping(kPost, "path", "cmd"));
  HttpHandler handler(
      scoped_ptr<CommandExecutor>(new DummyExecutor(kUnknownCommand)),
      map.Pass(), "/");
  HttpRequest request(kPost, "/path", "");
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kNotImplemented, response.status());
}

TEST(HttpHandlerTest, HandleCommand) {
  scoped_ptr<HttpHandler::CommandMap> map(new HttpHandler::CommandMap());
  map->push_back(CommandMapping(kPost, "path", "cmd"));
  HttpHandler handler(
      scoped_ptr<CommandExecutor>(new DummyExecutor()),
      map.Pass(), "/");
  HttpRequest request(kPost, "/path", "");
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kOk, response.status());
  std::string mime;
  ASSERT_TRUE(response.GetHeader("Content-Type", &mime));
  base::DictionaryValue body;
  body.SetInteger("status", kOk);
  body.SetInteger("value", 1);
  body.SetString("sessionId", "session_id");
  std::string json;
  base::JSONWriter::Write(&body, &json);
  ASSERT_STREQ(json.c_str(), response.body().c_str());
}

TEST(MatchesCommandTest, DiffMethod) {
  CommandMapping command(kPost, "path", "command");
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_FALSE(internal::MatchesCommand(
      kGet, "path", command, &session_id, &params));
  ASSERT_STREQ("", session_id.c_str());
  ASSERT_EQ(0u, params.size());
}

TEST(MatchesCommandTest, DiffPathLength) {
  CommandMapping command(kPost, "path/path", "command");
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_FALSE(internal::MatchesCommand(
      kPost, "path", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      kPost, "", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      kPost, "/", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      kPost, "path/path/path", command, &session_id, &params));
}

TEST(MatchesCommandTest, DiffPaths) {
  CommandMapping command(kPost, "path/apath", "command");
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_FALSE(internal::MatchesCommand(
      kPost, "path/bpath", command, &session_id, &params));
}

TEST(MatchesCommandTest, Substitution) {
  CommandMapping command(kPost, "path/:sessionId/space/:a/:b", "command");
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_TRUE(internal::MatchesCommand(
      kPost, "path/1/space/2/3", command, &session_id, &params));
  ASSERT_EQ("1", session_id);
  ASSERT_EQ(2u, params.size());
  std::string param;
  ASSERT_TRUE(params.GetString("a", &param));
  ASSERT_EQ("2", param);
  ASSERT_TRUE(params.GetString("b", &param));
  ASSERT_EQ("3", param);
}
