// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/server/http_handler.h"
#include "chrome/test/chromedriver/server/http_response.h"
#include "net/server/http_server_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

Status DummyCommand(
    Status status,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* value,
    std::string* out_session_id) {
  value->reset(new base::FundamentalValue(1));
  *out_session_id = "session_id";
  return status;
}

}  // namespace

TEST(HttpHandlerTest, HandleOutsideOfBaseUrl) {
  Logger log;
  HttpHandler handler(&log, "base/url/");
  net::HttpServerRequestInfo request;
  request.method = "get";
  request.path = "base/path";
  request.data = "body";
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kBadRequest, response.status());
}

TEST(HttpHandlerTest, HandleUnknownCommand) {
  Logger log;
  HttpHandler handler(&log, "/");
  net::HttpServerRequestInfo request;
  request.method = "get";
  request.path = "/path";
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kNotFound, response.status());
}

TEST(HttpHandlerTest, HandleNewSession) {
  Logger log;
  HttpHandler handler(&log, "/base/");
  handler.command_map_.reset(new HttpHandler::CommandMap());
  handler.command_map_->push_back(
      CommandMapping(kPost, internal::kNewSessionPathPattern,
                     base::Bind(&DummyCommand, Status(kOk))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/base/session";
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kSeeOther, response.status());
  std::string location;
  ASSERT_TRUE(response.GetHeader("Location", &location));
  std::string prefix = "/base/session/";
  ASSERT_EQ(prefix, location.substr(0, prefix.length()));
}

TEST(HttpHandlerTest, HandleInvalidPost) {
  Logger log;
  HttpHandler handler(&log, "/");
  handler.command_map_->push_back(
      CommandMapping(kPost, "path", base::Bind(&DummyCommand, Status(kOk))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/path";
  request.data = "should be a dictionary";
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kBadRequest, response.status());
}

TEST(HttpHandlerTest, HandleUnimplementedCommand) {
  Logger log;
  HttpHandler handler(&log, "/");
  handler.command_map_->push_back(
      CommandMapping(kPost, "path",
                     base::Bind(&DummyCommand, Status(kUnknownCommand))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/path";
  HttpResponse response;
  handler.Handle(request, &response);
  ASSERT_EQ(HttpResponse::kNotImplemented, response.status());
}

TEST(HttpHandlerTest, HandleCommand) {
  Logger log;
  HttpHandler handler(&log, "/");
  handler.command_map_->push_back(
      CommandMapping(kPost, "path", base::Bind(&DummyCommand, Status(kOk))));
  net::HttpServerRequestInfo request;
  request.method = "post";
  request.path = "/path";
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
  CommandMapping command(kPost, "path", base::Bind(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_FALSE(internal::MatchesCommand(
      "get", "path", command, &session_id, &params));
  ASSERT_STREQ("", session_id.c_str());
  ASSERT_EQ(0u, params.size());
}

TEST(MatchesCommandTest, DiffPathLength) {
  CommandMapping command(kPost, "path/path",
                         base::Bind(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_FALSE(internal::MatchesCommand(
      "post", "path", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      "post", std::string(), command, &session_id, &params));
  ASSERT_FALSE(
      internal::MatchesCommand("post", "/", command, &session_id, &params));
  ASSERT_FALSE(internal::MatchesCommand(
      "post", "path/path/path", command, &session_id, &params));
}

TEST(MatchesCommandTest, DiffPaths) {
  CommandMapping command(kPost, "path/apath",
                         base::Bind(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_FALSE(internal::MatchesCommand(
      "post", "path/bpath", command, &session_id, &params));
}

TEST(MatchesCommandTest, Substitution) {
  CommandMapping command(kPost, "path/:sessionId/space/:a/:b",
                         base::Bind(&DummyCommand, Status(kOk)));
  std::string session_id;
  base::DictionaryValue params;
  ASSERT_TRUE(internal::MatchesCommand(
      "post", "path/1/space/2/3", command, &session_id, &params));
  ASSERT_EQ("1", session_id);
  ASSERT_EQ(2u, params.size());
  std::string param;
  ASSERT_TRUE(params.GetString("a", &param));
  ASSERT_EQ("2", param);
  ASSERT_TRUE(params.GetString("b", &param));
  ASSERT_EQ("3", param);
}
