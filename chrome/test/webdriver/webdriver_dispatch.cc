// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_dispatch.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "chrome/test/webdriver/commands/command.h"
#include "chrome/test/webdriver/http_response.h"
#include "chrome/test/webdriver/webdriver_logging.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"
#include "chrome/test/webdriver/webdriver_util.h"

namespace webdriver {

namespace {

// Maximum safe size of HTTP response message. Any larger than this,
// the message may not be transferred at all.
const size_t kMaxHttpMessageSize = 1024 * 1024 * 16;  // 16MB

bool ForbidsMessageBody(const std::string& request_method,
                        const HttpResponse& response) {
  return request_method == "HEAD" ||
         response.status() == HttpResponse::kNoContent ||
         response.status() == HttpResponse::kNotModified ||
         (response.status() >= 100 && response.status() < 200);
}

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
      content_length = atoi(request_info->http_headers[header_index].value);
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

void DispatchCommand(Command* const command,
                     const std::string& method,
                     Response* response) {
  if (!command->Init(response))
    return;

  if (method == "POST") {
    command->ExecutePost(response);
  } else if (method == "GET") {
    command->ExecuteGet(response);
  } else if (method == "DELETE") {
    command->ExecuteDelete(response);
  } else {
    NOTREACHED();
  }
  command->Finish();
}

void SendOkWithBody(struct mg_connection* connection,
                    const std::string& content) {
  const char* response_fmt = "HTTP/1.1 200 OK\r\n"
                             "Content-Length:%d\r\n\r\n"
                             "%s";
  std::string response = base::StringPrintf(
      response_fmt, content.length(), content.c_str());
  mg_write(connection, response.data(), response.length());
}

void Shutdown(struct mg_connection* connection,
              const struct mg_request_info* request_info,
              void* user_data) {
  base::WaitableEvent* shutdown_event =
      reinterpret_cast<base::WaitableEvent*>(user_data);
  mg_printf(connection, "HTTP/1.1 200 OK\r\n\r\n\r\n");
  shutdown_event->Signal();
}

void SendHealthz(struct mg_connection* connection,
                 const struct mg_request_info* request_info,
                 void* user_data) {
  SendOkWithBody(connection, "ok");
}

void SendLog(struct mg_connection* connection,
             const struct mg_request_info* request_info,
             void* user_data) {
  std::string content, log;
  if (GetLogContents(&log)) {
    content = "START ChromeDriver log";
    const size_t kMaxSizeWithoutHeaders = kMaxHttpMessageSize - 10000;
    if (log.size() > kMaxSizeWithoutHeaders) {
      log = log.substr(log.size() - kMaxSizeWithoutHeaders);
      content += " (only last several MB)";
    }
    content += ":\n" + log + "END ChromeDriver log";
  } else {
    content = "No ChromeDriver log found";
  }
  SendOkWithBody(connection, content);
}

void SimulateHang(struct mg_connection* connection,
                  const struct mg_request_info* request_info,
                  void* user_data) {
  base::PlatformThread::Sleep(1000 * 60 * 5);
}

void SendNoContentResponse(struct mg_connection* connection,
                           const struct mg_request_info* request_info,
                           void* user_data) {
  std::string response = "HTTP/1.1 204 No Content\r\n"
                         "Content-Length:0\r\n"
                         "\r\n";
  mg_write(connection, response.data(), response.length());
}

void SendForbidden(struct mg_connection* connection,
                   const struct mg_request_info* request_info,
                   void* user_data) {
  mg_printf(connection, "HTTP/1.1 403 Forbidden\r\n\r\n");
}

void SendNotImplementedError(struct mg_connection* connection,
                             const struct mg_request_info* request_info,
                             void* user_data) {
  // Send a well-formed WebDriver JSON error response to ensure clients
  // handle it correctly.
  std::string body = base::StringPrintf(
      "{\"status\":%d,\"value\":{\"message\":"
      "\"Command has not been implemented yet: %s %s\"}}",
      kUnknownCommand, request_info->request_method, request_info->uri);

  std::string header = base::StringPrintf(
      "HTTP/1.1 501 Not Implemented\r\n"
      "Content-Type:application/json\r\n"
      "Content-Length:%" PRIuS "\r\n"
      "\r\n", body.length());

  mg_write(connection, header.data(), header.length());
  mg_write(connection, body.data(), body.length());
}

}  // namespace

namespace internal {

void PrepareHttpResponse(const Response& command_response,
                         HttpResponse* const http_response) {
  ErrorCode status = command_response.GetStatus();
  switch (status) {
    case kSuccess:
      http_response->set_status(HttpResponse::kOk);
      break;

    // TODO(jleyba): kSeeOther, kBadRequest, kSessionNotFound,
    // and kMethodNotAllowed should be detected before creating
    // a command_response, and should thus not need conversion.
    case kSeeOther: {
      const Value* const value = command_response.GetValue();
      std::string location;
      if (!value->GetAsString(&location)) {
        // This should never happen.
        http_response->set_status(HttpResponse::kInternalServerError);
        http_response->SetBody("Unable to set 'Location' header: response "
                               "value is not a string: " +
                               command_response.ToJSON());
        return;
      }
      http_response->AddHeader("Location", location);
      http_response->set_status(HttpResponse::kSeeOther);
      break;
    }

    case kBadRequest:
    case kSessionNotFound:
      http_response->set_status(status);
      break;

    case kMethodNotAllowed: {
      const Value* const value = command_response.GetValue();
      if (!value->IsType(Value::TYPE_LIST)) {
        // This should never happen.
        http_response->set_status(HttpResponse::kInternalServerError);
        http_response->SetBody(
            "Unable to set 'Allow' header: response value was "
            "not a list of strings: " + command_response.ToJSON());
        return;
      }

      const ListValue* const list_value =
          static_cast<const ListValue* const>(value);
      std::vector<std::string> allowed_methods;
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        std::string method;
        if (list_value->GetString(i, &method)) {
          allowed_methods.push_back(method);
        } else {
          // This should never happen.
          http_response->set_status(HttpResponse::kInternalServerError);
          http_response->SetBody(
              "Unable to set 'Allow' header: response value was "
              "not a list of strings: " + command_response.ToJSON());
          return;
        }
      }
      http_response->AddHeader("Allow", JoinString(allowed_methods, ','));
      http_response->set_status(HttpResponse::kMethodNotAllowed);
      break;
    }

    // All other errors should be treated as generic 500s. The client
    // will be responsible for inspecting the message body for details.
    case kInternalServerError:
    default:
      http_response->set_status(HttpResponse::kInternalServerError);
      break;
  }

  http_response->SetMimeType("application/json; charset=utf-8");
  http_response->SetBody(command_response.ToJSON());
}

void SendResponse(struct mg_connection* const connection,
                  const std::string& request_method,
                  const Response& response) {
  HttpResponse http_response;
  PrepareHttpResponse(response, &http_response);

  std::string message_header = base::StringPrintf("HTTP/1.1 %d %s\r\n",
      http_response.status(), http_response.GetReasonPhrase().c_str());

  typedef HttpResponse::HeaderMap::const_iterator HeaderIter;
  for (HeaderIter header = http_response.headers()->begin();
       header != http_response.headers()->end();
       ++header) {
    message_header.append(base::StringPrintf("%s:%s\r\n",
        header->first.c_str(), header->second.c_str()));
  }
  message_header.append("\r\n");

  mg_write(connection, message_header.data(), message_header.length());
  if (!ForbidsMessageBody(request_method, http_response))
    mg_write(connection, http_response.data(), http_response.length());
}

bool ParseRequestInfo(const struct mg_request_info* const request_info,
                      struct mg_connection* const connection,
                      std::string* method,
                      std::vector<std::string>* path_segments,
                      DictionaryValue** parameters,
                      Response* const response) {
  *method = request_info->request_method;
  if (*method == "HEAD")
    *method = "GET";
  else if (*method == "PUT")
    *method = "POST";

  std::string uri(request_info->uri);
  SessionManager* manager = SessionManager::GetInstance();
  uri = uri.substr(manager->url_base().length());

  base::SplitString(uri, '/', path_segments);

  if (*method == "POST") {
    std::string json;
    ReadRequestBody(request_info, connection, &json);
    if (json.length() > 0) {
      std::string error_msg;
      scoped_ptr<Value> params(base::JSONReader::ReadAndReturnError(
          json, true, NULL, &error_msg));
      if (!params.get()) {
        response->SetError(new Error(
            kBadRequest,
            "Failed to parse command data: " + error_msg +
                "\n  Data: " + json));
        return false;
      }
      if (!params->IsType(Value::TYPE_DICTIONARY)) {
        response->SetError(new Error(
            kBadRequest,
            "Data passed in URL must be a dictionary. Data: " + json));
        return false;
      }
      *parameters = static_cast<DictionaryValue*>(params.release());
    }
  }
  return true;
}

void DispatchHelper(Command* command_ptr,
                    const std::string& method,
                    Response* response) {
  CHECK(method == "GET" || method == "POST" || method == "DELETE");
  scoped_ptr<Command> command(command_ptr);

  if ((method == "GET" && !command->DoesGet()) ||
      (method == "POST" && !command->DoesPost()) ||
      (method == "DELETE" && !command->DoesDelete())) {
    ListValue* methods = new ListValue;
    if (command->DoesPost())
      methods->Append(Value::CreateStringValue("POST"));
    if (command->DoesGet()) {
      methods->Append(Value::CreateStringValue("GET"));
      methods->Append(Value::CreateStringValue("HEAD"));
    }
    if (command->DoesDelete())
      methods->Append(Value::CreateStringValue("DELETE"));
    response->SetStatus(kMethodNotAllowed);
    response->SetValue(methods);
    return;
  }

  DispatchCommand(command.get(), method, response);
}

}  // namespace internal

Dispatcher::Dispatcher(const std::string& url_base)
    : url_base_(url_base) {
  // Overwrite mongoose's default handler for /favicon.ico to always return a
  // 204 response so we don't spam the logs with 404s.
  AddCallback("/favicon.ico", &SendNoContentResponse, NULL);
  AddCallback("/hang", &SimulateHang, NULL);
}

Dispatcher::~Dispatcher() {}

void Dispatcher::AddShutdown(const std::string& pattern,
                             base::WaitableEvent* shutdown_event) {
  AddCallback(url_base_ + pattern, &Shutdown, shutdown_event);
}

void Dispatcher::AddHealthz(const std::string& pattern) {
  AddCallback(url_base_ + pattern, &SendHealthz, NULL);
}

void Dispatcher::AddLog(const std::string& pattern) {
  AddCallback(url_base_ + pattern, &SendLog, NULL);
}

void Dispatcher::SetNotImplemented(const std::string& pattern) {
  AddCallback(url_base_ + pattern, &SendNotImplementedError, NULL);
}

void Dispatcher::ForbidAllOtherRequests() {
  AddCallback("*", &SendForbidden, NULL);
}

void Dispatcher::AddCallback(const std::string& uri_pattern,
                             webdriver::mongoose::HttpCallback callback,
                             void* user_data) {
  callbacks_.push_back(webdriver::mongoose::CallbackDetails(
    uri_pattern,
    callback,
    user_data));
}


bool Dispatcher::ProcessHttpRequest(
    struct mg_connection* connection,
    const struct mg_request_info* request_info) {
  std::vector<webdriver::mongoose::CallbackDetails>::const_iterator callback;
  for (callback = callbacks_.begin();
       callback < callbacks_.end();
       ++callback) {
    if (MatchPattern(request_info->uri, callback->uri_regex_)) {
      callback->func_(connection, request_info, callback->user_data_);
      return true;
    }
  }
  return false;
}

}  // namespace webdriver
