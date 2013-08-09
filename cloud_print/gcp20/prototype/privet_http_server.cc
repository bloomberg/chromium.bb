// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/privet_http_server.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/socket/tcp_listen_socket.h"
#include "url/gurl.h"

namespace {

const int kDeviceBusyTimeout = 30;  // in seconds
const int kPendingUserActionTimeout = 5;  // in seconds

// {"error":|error_type|}
scoped_ptr<base::DictionaryValue> CreateError(const std::string& error_type) {
  scoped_ptr<base::DictionaryValue> error(new base::DictionaryValue);
  error->SetString("error", error_type);

  return error.Pass();
}

// {"error":|error_type|, "description":|description|}
scoped_ptr<base::DictionaryValue> CreateErrorWithDescription(
    const std::string& error_type,
    const std::string& description) {
  scoped_ptr<base::DictionaryValue> error(CreateError(error_type));
  error->SetString("description", description);
  return error.Pass();
}

// {"error":|error_type|, "timeout":|timout|}
scoped_ptr<base::DictionaryValue> CreateErrorWithTimeout(
    const std::string& error_type,
    int timeout) {
  scoped_ptr<base::DictionaryValue> error(CreateError(error_type));
  error->SetInteger("timeout", timeout);
  return error.Pass();
}

// Returns |true| if |request| should be GET method.
bool IsGetMethod(const std::string& request) {
  return request == "/privet/info"/* ||
         request == "/privet/accesstoken" ||
         request == "/privet/capabilities" ||
         request == "/privet/printer/jobstate"*/;
}

// Returns |true| if |request| should be POST method.
bool IsPostMethod(const std::string& request) {
  return request == "/privet/register"/* ||
         request == "/privet/printer/createjob" ||
         request == "/privet/printer/submitdoc"*/;
}

}  // namespace

PrivetHttpServer::DeviceInfo::DeviceInfo() : uptime(0) {
}

PrivetHttpServer::DeviceInfo::~DeviceInfo() {
}

PrivetHttpServer::PrivetHttpServer(Delegate* delegate)
    : port_(0),
      delegate_(delegate) {
}

PrivetHttpServer::~PrivetHttpServer() {
  Shutdown();
}

bool PrivetHttpServer::Start(uint16 port) {
  if (server_)
    return true;

  net::TCPListenSocketFactory factory("0.0.0.0", port);
  server_ = new net::HttpServer(factory, this);
  net::IPEndPoint address;

  if (server_->GetLocalAddress(&address) != net::OK) {
    NOTREACHED() << "Cannot start HTTP server";
    return false;
  }

  VLOG(1) << "Address of HTTP server: " << address.ToString();
  return true;
}

void PrivetHttpServer::Shutdown() {
  if (!server_)
    return;

  server_ = NULL;
}

void PrivetHttpServer::OnHttpRequest(int connection_id,
                                     const net::HttpServerRequestInfo& info) {
  VLOG(1) << "Processing HTTP request: " << info.path;
  GURL url("http://host" + info.path);

  if (!ValidateRequestMethod(connection_id, url.path(), info.method))
    return;

  if (!CommandLine::ForCurrentProcess()->HasSwitch("disable-x-token")) {
    net::HttpServerRequestInfo::HeadersMap::const_iterator iter =
        info.headers.find("x-privet-token");
    if (iter == info.headers.end()) {
      server_->Send(connection_id, net::HTTP_BAD_REQUEST,
                    "Missing X-Privet-Token header.\n"
                    "TODO: Message should be in header, not in the body!",
                    "text/plain");
      return;
    }

    if (url.path() != "/privet/info" &&
        !delegate_->CheckXPrivetTokenHeader(iter->second)) {
      server_->Send(connection_id, net::HTTP_OK,
                    "{\"error\":\"invalid_x_privet_token\"}",
                    "application/json");
      return;
    }
  }

  std::string response;
  net::HttpStatusCode status_code =
      ProcessHttpRequest(url, info.data, &response);

  server_->Send(connection_id, status_code, response, "application/json");
}

void PrivetHttpServer::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
}

void PrivetHttpServer::OnWebSocketMessage(int connection_id,
                                          const std::string& data) {
}

void PrivetHttpServer::OnClose(int connection_id) {
}

void PrivetHttpServer::ReportInvalidMethod(int connection_id) {
  server_->Send(connection_id, net::HTTP_BAD_REQUEST, "Invalid method",
                "text/plain");
}

bool PrivetHttpServer::ValidateRequestMethod(int connection_id,
                                             const std::string& request,
                                             const std::string& method) {
  DCHECK(!IsGetMethod(request) || !IsPostMethod(request));

  if (!IsGetMethod(request) && !IsPostMethod(request)) {
    server_->Send404(connection_id);
    return false;
  }

  if (CommandLine::ForCurrentProcess()->HasSwitch("disable-method-check"))
    return true;

  if ((IsGetMethod(request) && method != "GET") ||
      (IsPostMethod(request) && method != "POST")) {
    ReportInvalidMethod(connection_id);
    return false;
  }

  return true;
}

net::HttpStatusCode PrivetHttpServer::ProcessHttpRequest(
    const GURL& url,
    const std::string& data,
    std::string* response) {
  net::HttpStatusCode status_code = net::HTTP_OK;
  scoped_ptr<base::DictionaryValue> json_response;

  if (url.path() == "/privet/info") {
    json_response = ProcessInfo(&status_code);
  } else if (url.path() == "/privet/register") {
    json_response = ProcessRegister(url, &status_code);
  } else {
    NOTREACHED();
  }

  if (!json_response) {
    response->clear();
    return status_code;
  }

  base::JSONWriter::WriteWithOptions(json_response.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     response);
  return status_code;
}

// Privet API methods:

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessInfo(
    net::HttpStatusCode* status_code) const {

  DeviceInfo info;
  delegate_->CreateInfo(&info);

  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);
  response->SetString("version", info.version);
  response->SetString("name", info.name);
  response->SetString("description", info.description);
  response->SetString("url", info.url);
  response->SetString("id", info.id);
  response->SetString("device_state", info.device_state);
  response->SetString("connection_state", info.connection_state);
  response->SetString("manufacturer", info.manufacturer);
  response->SetString("model", info.model);
  response->SetString("serial_number", info.serial_number);
  response->SetString("firmware", info.firmware);
  response->SetInteger("uptime", info.uptime);
  response->SetString("x-privet-token", info.x_privet_token);

  base::ListValue api;
  for (size_t i = 0; i < info.api.size(); ++i)
    api.AppendString(info.api[i]);
  response->Set("api", api.DeepCopy());

  base::ListValue type;
  for (size_t i = 0; i < info.type.size(); ++i)
    type.AppendString(info.type[i]);
  response->Set("type", type.DeepCopy());

  *status_code = net::HTTP_OK;
  return response.Pass();
}

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessRegister(
    const GURL& url,
    net::HttpStatusCode* status_code) {
  if (delegate_->IsRegistered()) {
    *status_code = net::HTTP_NOT_FOUND;
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string action;
  std::string user;
  bool params_present =
      net::GetValueForKeyInQuery(url, "action", &action) &&
      net::GetValueForKeyInQuery(url, "user", &user) &&
      !user.empty();

  RegistrationErrorStatus status = REG_ERROR_INVALID_PARAMS;
  scoped_ptr<base::DictionaryValue> response(new DictionaryValue);

  if (params_present) {
    response->SetString("action", action);
    response->SetString("user", user);

    if (action == "start")
      status = delegate_->RegistrationStart(user);

    if (action == "getClaimToken") {
      std::string token;
      std::string claim_url;
      status = delegate_->RegistrationGetClaimToken(user, &token, &claim_url);
      response->SetString("token", token);
      response->SetString("claim_url", claim_url);
    }

    if (action == "complete") {
      std::string device_id;
      status = delegate_->RegistrationComplete(user, &device_id);
      response->SetString("device_id", device_id);
    }

    if (action == "cancel")
      status = delegate_->RegistrationCancel(user);
  }

  if (status != REG_ERROR_OK)
    response.reset();

  ProcessRegistrationStatus(status, &response);
  *status_code = net::HTTP_OK;
  return response.Pass();
}

void PrivetHttpServer::ProcessRegistrationStatus(
    RegistrationErrorStatus status,
    scoped_ptr<base::DictionaryValue>* current_response) const {
  switch (status) {
    case REG_ERROR_OK:
      DCHECK(*current_response) << "Response shouldn't be empty.";
      break;

    case REG_ERROR_INVALID_PARAMS:
      *current_response = CreateError("invalid_params");
      break;
    case REG_ERROR_DEVICE_BUSY:
      *current_response = CreateErrorWithTimeout("device_busy",
                                                 kDeviceBusyTimeout);
      break;
    case REG_ERROR_PENDING_USER_ACTION:
      *current_response = CreateErrorWithTimeout("pending_user_action",
                                                 kPendingUserActionTimeout);
      break;
    case REG_ERROR_USER_CANCEL:
      *current_response = CreateError("user_cancel");
      break;
    case REG_ERROR_CONFIRMATION_TIMEOUT:
      *current_response = CreateError("confirmation_timeout");
      break;
    case REG_ERROR_INVALID_ACTION:
      *current_response = CreateError("invalid_action");
      break;
    case REG_ERROR_SERVER_ERROR: {
      std::string description;
      delegate_->GetRegistrationServerError(&description);
      *current_response = CreateErrorWithDescription("server_error",
                                                     description);
      break;
    }

    default:
      NOTREACHED();
  };
}


