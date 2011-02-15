// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/dispatch.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/test/webdriver/http_response.h"
#include "chrome/test/webdriver/commands/command.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/utility_functions.h"

namespace webdriver {

namespace {

bool ForbidsMessageBody(const std::string& request_method,
                        const HttpResponse& response) {
  return request_method == "HEAD" ||
         response.status() == HttpResponse::kNoContent ||
         response.status() == HttpResponse::kNotModified ||
         (response.status() >= 100 && response.status() < 200);
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
}

}  // namespace

namespace internal {

void PrepareHttpResponse(const Response& command_response,
                         HttpResponse* const http_response) {
  switch (command_response.status()) {
    case kSuccess:
      http_response->set_status(HttpResponse::kOk);
      break;

    // TODO(jleyba): kSeeOther, kBadRequest, kSessionNotFound,
    // and kMethodNotAllowed should be detected before creating
    // a command_response, and should thus not need conversion.
    case kSeeOther: {
      const Value* const value = command_response.value();
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
      http_response->set_status(command_response.status());
      break;

    case kMethodNotAllowed: {
      const Value* const value = command_response.value();
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
  base::SplitString(uri, '/', path_segments);

  if (*method == "POST" && request_info->post_data_len > 0) {
    VLOG(1) << "...parsing request body";
    std::string json(request_info->post_data, request_info->post_data_len);
    std::string error;
    if (!ParseJSONDictionary(json, parameters, &error)) {
      response->set_value(Value::CreateStringValue(
          "Failed to parse command data: " + error + "\n  Data: " + json));
      response->set_status(kBadRequest);
      return false;
    }
  }
  VLOG(1) << "Parsed " << method << " " << uri
        << std::string(request_info->post_data, request_info->post_data_len);
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
    response->set_status(kMethodNotAllowed);
    response->set_value(methods);
    return;
  }

  DispatchCommand(command.get(), method, response);
}

}  // namespace internal

}  // namespace webdriver
