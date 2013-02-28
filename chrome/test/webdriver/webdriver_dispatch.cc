// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_dispatch.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/webdriver/commands/command.h"
#include "chrome/test/webdriver/http_response.h"
#include "chrome/test/webdriver/webdriver_logging.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"
#include "chrome/test/webdriver/webdriver_switches.h"
#include "chrome/test/webdriver/webdriver_util.h"

namespace webdriver {

namespace {

// Maximum safe size of HTTP response message. Any larger than this,
// the message may not be transferred at all.
const size_t kMaxHttpMessageSize = 1024 * 1024 * 16;  // 16MB

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

void WriteHttpResponse(struct mg_connection* connection,
                       const HttpResponse& response) {
  HttpResponse modified_response(response);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kEnableKeepAlive))
    modified_response.AddHeader("connection", "close");
  std::string data;
  modified_response.GetData(&data);
  mg_write(connection, data.data(), data.length());
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
  command->Finish(response);
}

void SendOkWithBody(struct mg_connection* connection,
                    const std::string& content) {
  HttpResponse response;
  response.set_body(content);
  WriteHttpResponse(connection, response);
}

void Shutdown(struct mg_connection* connection,
              const struct mg_request_info* request_info,
              void* user_data) {
  base::WaitableEvent* shutdown_event =
      reinterpret_cast<base::WaitableEvent*>(user_data);
  WriteHttpResponse(connection, HttpResponse());
  shutdown_event->Signal();
}

void SendStatus(struct mg_connection* connection,
                const struct mg_request_info* request_info,
                void* user_data) {
  chrome::VersionInfo version_info;
  base::DictionaryValue* build_info = new base::DictionaryValue;
  build_info->SetString("time",
                        base::StringPrintf("%s %s PST", __DATE__, __TIME__));
  build_info->SetString("version", version_info.Version());
  build_info->SetString("revision", version_info.LastChange());

  base::DictionaryValue* os_info = new base::DictionaryValue;
  os_info->SetString("name", base::SysInfo::OperatingSystemName());
  os_info->SetString("version", base::SysInfo::OperatingSystemVersion());
  os_info->SetString("arch", base::SysInfo::OperatingSystemArchitecture());

  base::DictionaryValue* status = new base::DictionaryValue;
  status->Set("build", build_info);
  status->Set("os", os_info);

  Response response;
  response.SetStatus(kSuccess);
  response.SetValue(status);  // Assumes ownership of |status|.

  internal::SendResponse(connection,
                         request_info->request_method,
                         response);
}

void SendLog(struct mg_connection* connection,
             const struct mg_request_info* request_info,
             void* user_data) {
  std::string content, log;
  if (FileLog::Get()->GetLogContents(&log)) {
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
  base::PlatformThread::Sleep(base::TimeDelta::FromMinutes(5));
}

void SendNoContentResponse(struct mg_connection* connection,
                           const struct mg_request_info* request_info,
                           void* user_data) {
  WriteHttpResponse(connection, HttpResponse(HttpResponse::kNoContent));
}

void SendForbidden(struct mg_connection* connection,
                   const struct mg_request_info* request_info,
                   void* user_data) {
  WriteHttpResponse(connection, HttpResponse(HttpResponse::kForbidden));
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

  HttpResponse response(HttpResponse::kNotImplemented);
  response.AddHeader("Content-Type", "application/json");
  response.set_body(body);
  WriteHttpResponse(connection, response);
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
      const base::Value* const value = command_response.GetValue();
      std::string location;
      if (!value->GetAsString(&location)) {
        // This should never happen.
        http_response->set_status(HttpResponse::kInternalServerError);
        http_response->set_body("Unable to set 'Location' header: response "
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
      const base::Value* const value = command_response.GetValue();
      if (!value->IsType(base::Value::TYPE_LIST)) {
        // This should never happen.
        http_response->set_status(HttpResponse::kInternalServerError);
        http_response->set_body(
            "Unable to set 'Allow' header: response value was "
            "not a list of strings: " + command_response.ToJSON());
        return;
      }

      const base::ListValue* const list_value =
          static_cast<const base::ListValue* const>(value);
      std::vector<std::string> allowed_methods;
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        std::string method;
        if (list_value->GetString(i, &method)) {
          allowed_methods.push_back(method);
        } else {
          // This should never happen.
          http_response->set_status(HttpResponse::kInternalServerError);
          http_response->set_body(
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
  http_response->set_body(command_response.ToJSON());
}

void SendResponse(struct mg_connection* const connection,
                  const std::string& request_method,
                  const Response& response) {
  HttpResponse http_response;
  PrepareHttpResponse(response, &http_response);
  WriteHttpResponse(connection, http_response);
}

bool ParseRequestInfo(const struct mg_request_info* const request_info,
                      struct mg_connection* const connection,
                      std::string* method,
                      std::vector<std::string>* path_segments,
                      base::DictionaryValue** parameters,
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
      scoped_ptr<base::Value> params(base::JSONReader::ReadAndReturnError(
          json, base::JSON_ALLOW_TRAILING_COMMAS, NULL, &error_msg));
      if (!params.get()) {
        response->SetError(new Error(
            kBadRequest,
            "Failed to parse command data: " + error_msg +
                "\n  Data: " + json));
        return false;
      }
      if (!params->IsType(base::Value::TYPE_DICTIONARY)) {
        response->SetError(new Error(
            kBadRequest,
            "Data passed in URL must be a dictionary. Data: " + json));
        return false;
      }
      *parameters = static_cast<base::DictionaryValue*>(params.release());
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
    base::ListValue* methods = new base::ListValue;
    if (command->DoesPost())
      methods->Append(new base::StringValue("POST"));
    if (command->DoesGet()) {
      methods->Append(new base::StringValue("GET"));
      methods->Append(new base::StringValue("HEAD"));
    }
    if (command->DoesDelete())
      methods->Append(new base::StringValue("DELETE"));
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

void Dispatcher::AddStatus(const std::string& pattern) {
  AddCallback(url_base_ + pattern, &SendStatus, NULL);
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
