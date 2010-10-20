// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/test/webdriver/dispatch.h"

#include <sstream>
#include <string>

#include "base/json/json_writer.h"
#include "chrome/test/webdriver/commands/command.h"

namespace webdriver {

// The standard HTTP Status codes are implemented below.  Chrome uses
// OK, See Other, Not Found, Method Not Allowed, and Internal Error.
// Internal Error, HTTP 500, is used as a catch all for any issue
// not covered in the JSON protocol.
void SendHttpOk(struct mg_connection* const connection,
                const struct mg_request_info* const request_info,
                const Response& response) {
  const std::string json = response.ToJSON();
  std::ostringstream out;
  out << "HTTP/1.1 200 OK\r\n"
      << "Content-Length: " << strlen(json.c_str()) << "\r\n"
      << "Content-Type: application/json; charset=UTF-8\r\n"
      << "Vary: Accept-Charset, Accept-Encoding, Accept-Language, Accept\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n\r\n";
  if (strcmp(request_info->request_method, "HEAD") != 0)
    out << json << "\r\n";
  VLOG(1) << out.str();
  mg_printf(connection, "%s", out.str().c_str());
}

void SendHttpSeeOther(struct mg_connection* const connection,
                      const struct mg_request_info* const request_info,
                      const Response& response) {
  const Value* value = response.value();
  CheckValueType(Value::TYPE_STRING, value);
  std::string location;
  if (!value->GetAsString(&location)) {
    NOTREACHED();
  }

  std::ostringstream out;
  out << "HTTP/1.1 303 See Other\r\n"
      << "Location: " << location << "\r\n"
      << "Content-Type: text/html\r\n"
      << "Content-Length: 0\r\n\r\n";
  VLOG(1) << out.str();
  mg_printf(connection, "%s", out.str().c_str());
}

void SendHttpBadRequest(struct mg_connection* const connection,
                        const struct mg_request_info* const request_info,
                        const Response& response) {
  const std::string json = response.ToJSON();
  std::ostringstream out;
  out << "HTTP/1.1 400 Bad Request\r\n"
      << "Content-Length: " << strlen(json.c_str()) << "\r\n"
      << "Content-Type: application/json; charset=UTF-8\r\n"
      << "Vary: Accept-Charset, Accept-Encoding, Accept-Language, Accept\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n\r\n";
  if (strcmp(request_info->request_method, "HEAD") != 0)
    out << json << "\r\n";
  VLOG(1) << out.str();
  mg_printf(connection, "%s", out.str().c_str());
}

void SendHttpNotFound(struct mg_connection* const connection,
                      const struct mg_request_info* const request_info,
                      const Response& response) {
  const std::string json = response.ToJSON();
  std::ostringstream out;
  out << "HTTP/1.1 404 Not Found\r\n"
      << "Content-Length: " << strlen(json.c_str()) << "\r\n"
      << "Content-Type: application/json; charset=UTF-8\r\n"
      << "Vary: Accept-Charset, Accept-Encoding, Accept-Language, Accept\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n\r\n";
  if (strcmp(request_info->request_method, "HEAD") != 0)
    out << json << "\r\n";
  VLOG(1) << out.str();
  mg_printf(connection, "%s", out.str().c_str());
}

void SendHttpMethodNotAllowed(struct mg_connection* const connection,
                              const struct mg_request_info* const request_info,
                              const Response& response) {
  const Value* value = response.value();
  CheckValueType(Value::TYPE_LIST, value);

  std::vector<std::string> allowed_methods;
  const ListValue* list_value = static_cast<const ListValue*>(value);
  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    std::string method;
    LOG_IF(WARNING, list_value->GetString(i, &method))
        << "Ignoring non-string value at index " << i;
    allowed_methods.push_back(method);
  }

  std::ostringstream out;
  out << "HTTP/1.1 405 Method Not Allowed\r\n"
      << "Content-Type: text/html\r\n"
      << "Content-Length: 0\r\n"
      << "Allow: " << JoinString(allowed_methods, ',') << "\r\n\r\n";
  VLOG(1) << out.str();
  mg_printf(connection, "%s", out.str().c_str());
}

void SendHttpInternalError(struct mg_connection* const connection,
                           const struct mg_request_info* const request_info,
                           const Response& response) {
  const std::string json = response.ToJSON();
  std::ostringstream out;
  out << "HTTP/1.1 500 Internal Server Error\r\n"
      << "Content-Length: " << strlen(json.c_str()) << "\r\n"
      << "Content-Type: application/json; charset=UTF-8\r\n"
      << "Vary: Accept-Charset, Accept-Encoding, Accept-Language, Accept\r\n"
      << "Accept-Ranges: bytes\r\n"
      << "Connection: close\r\n\r\n";
  if (strcmp(request_info->request_method, "HEAD") != 0)
    out << json << "\r\n";
  VLOG(1) << out.str();
  mg_printf(connection, "%s", out.str().c_str());
}

// For every HTTP request from the mongoode server DispatchCommand will
// inspect the HTTP method call requested and execute the proper function
// mapped from the class Command.
void DispatchCommand(Command* const command, const std::string& method,
                     Response* response) {
  if (command->DoesPost() && method == "POST") {
    if (command->Init(response))
      command->ExecutePost(response);
  } else if (command->DoesGet() && method == "GET") {
    if (command->Init(response))
      command->ExecuteGet(response);
  } else if (command->DoesDelete() && method == "DELETE") {
    if (command->Init(response))
      command->ExecuteDelete(response);
  } else {
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
    response->set_value(methods);  // Assumes ownership.
  }
}

void SendResponse(struct mg_connection* const connection,
                  const struct mg_request_info* const request_info,
                  const Response& response) {
  switch (response.status()) {
    case kSuccess:
      SendHttpOk(connection, request_info, response);
      break;

    case kSeeOther:
      SendHttpSeeOther(connection, request_info, response);
      break;

    case kBadRequest:
      SendHttpBadRequest(connection, request_info, response);
      break;

    case kSessionNotFound:
      SendHttpNotFound(connection, request_info, response);
      break;

    case kMethodNotAllowed:
      SendHttpMethodNotAllowed(connection, request_info, response);
      break;

    // All other errors should be treated as generic 500s. The client will be
    // responsible for inspecting the message body for details.
  case kInternalServerError:
  default:
      SendHttpInternalError(connection, request_info, response);
      break;
  }
}

}  // namespace webdriver

