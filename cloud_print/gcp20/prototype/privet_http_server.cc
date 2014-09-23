// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/privet_http_server.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "cloud_print/gcp20/prototype/gcp20_switches.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/socket/tcp_server_socket.h"
#include "url/gurl.h"

namespace {

const int kDeviceBusyTimeout = 30;  // in seconds
const int kPendingUserActionTimeout = 5;  // in seconds

const char kPrivetInfo[] = "/privet/info";
const char kPrivetRegister[] = "/privet/register";
const char kPrivetCapabilities[] = "/privet/capabilities";
const char kPrivetPrinterCreateJob[] = "/privet/printer/createjob";
const char kPrivetPrinterSubmitDoc[] = "/privet/printer/submitdoc";
const char kPrivetPrinterJobState[] = "/privet/printer/jobstate";

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

// {"error":|error_type|, "timeout":|timeout|}
scoped_ptr<base::DictionaryValue> CreateErrorWithTimeout(
    const std::string& error_type,
    int timeout) {
  scoped_ptr<base::DictionaryValue> error(CreateError(error_type));
  error->SetInteger("timeout", timeout);
  return error.Pass();
}

// Converts state to string.
std::string LocalPrintJobStateToString(LocalPrintJob::State state) {
  switch (state) {
    case LocalPrintJob::STATE_DRAFT:
      return "draft";
      break;
    case LocalPrintJob::STATE_ABORTED:
      return "done";
      break;
    case LocalPrintJob::STATE_DONE:
      return "done";
      break;
    default:
      NOTREACHED();
      return std::string();
  }
}


// Returns |true| if |request| should be GET method.
bool IsGetMethod(const std::string& request) {
  return request == kPrivetInfo ||
         request == kPrivetCapabilities ||
         request == kPrivetPrinterJobState;
}

// Returns |true| if |request| should be POST method.
bool IsPostMethod(const std::string& request) {
  return request == kPrivetRegister ||
         request == kPrivetPrinterCreateJob ||
         request == kPrivetPrinterSubmitDoc;
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

  scoped_ptr<net::ServerSocket> server_socket(
      new net::TCPServerSocket(NULL, net::NetLog::Source()));
  std::string listen_address = "::";
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableIpv6))
    listen_address = "0.0.0.0";
  server_socket->ListenWithAddressAndPort(listen_address, port, 1);
  server_.reset(new net::HttpServer(server_socket.Pass(), this));

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

  server_.reset(NULL);
}

void PrivetHttpServer::OnHttpRequest(int connection_id,
                                     const net::HttpServerRequestInfo& info) {
  VLOG(1) << "Processing HTTP request: " << info.path;
  GURL url("http://host" + info.path);

  if (!ValidateRequestMethod(connection_id, url.path(), info.method))
    return;

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableXTocken)) {
    net::HttpServerRequestInfo::HeadersMap::const_iterator iter =
        info.headers.find("x-privet-token");
    if (iter == info.headers.end()) {
      server_->Send(connection_id, net::HTTP_BAD_REQUEST,
                    "Missing X-Privet-Token header.\n"
                    "TODO: Message should be in header, not in the body!",
                    "text/plain");
      return;
    }

    if (url.path() != kPrivetInfo &&
        !delegate_->CheckXPrivetTokenHeader(iter->second)) {
      server_->Send(connection_id, net::HTTP_OK,
                    "{\"error\":\"invalid_x_privet_token\"}",
                    "application/json");
      return;
    }
  }

  std::string response;
  net::HttpStatusCode status_code = ProcessHttpRequest(url, info, &response);

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
    const net::HttpServerRequestInfo& info,
    std::string* response) {
  net::HttpStatusCode status_code = net::HTTP_OK;
  scoped_ptr<base::DictionaryValue> json_response;

  if (url.path() == kPrivetInfo) {
    json_response = ProcessInfo(&status_code);
  } else if (url.path() == kPrivetRegister) {
    json_response = ProcessRegister(url, &status_code);
  } else if (url.path() == kPrivetCapabilities) {
    json_response = ProcessCapabilities(&status_code);
  } else if (url.path() == kPrivetPrinterCreateJob) {
    json_response = ProcessCreateJob(url, info.data, &status_code);
  } else if (url.path() == kPrivetPrinterSubmitDoc) {
    json_response = ProcessSubmitDoc(url, info, &status_code);
  } else if (url.path() == kPrivetPrinterJobState) {
    json_response = ProcessJobState(url, &status_code);
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

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessCapabilities(
    net::HttpStatusCode* status_code) const {
  if (!delegate_->IsRegistered()) {
    *status_code = net::HTTP_NOT_FOUND;
    return scoped_ptr<base::DictionaryValue>();
  }
  return make_scoped_ptr(delegate_->GetCapabilities().DeepCopy());
}

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessCreateJob(
    const GURL& url,
    const std::string& body,
    net::HttpStatusCode* status_code) const {
  if (!delegate_->IsRegistered() || !delegate_->IsLocalPrintingAllowed()) {
    *status_code = net::HTTP_NOT_FOUND;
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string job_id;
  // TODO(maksymb): Use base::Time for expiration
  int expires_in = 0;
  // TODO(maksymb): Use base::TimeDelta for timeout values
  int error_timeout = -1;
  std::string error_description;

  LocalPrintJob::CreateResult result =
      delegate_->CreateJob(body, &job_id, &expires_in,
                           &error_timeout, &error_description);

  scoped_ptr<base::DictionaryValue> response;
  *status_code = net::HTTP_OK;
  switch (result) {
    case LocalPrintJob::CREATE_SUCCESS:
      response.reset(new base::DictionaryValue);
      response->SetString("job_id", job_id);
      response->SetInteger("expires_in", expires_in);
      return response.Pass();

    case LocalPrintJob::CREATE_INVALID_TICKET:
      return CreateError("invalid_ticket");
    case LocalPrintJob::CREATE_PRINTER_BUSY:
      return CreateErrorWithTimeout("printer_busy", error_timeout);
    case LocalPrintJob::CREATE_PRINTER_ERROR:
      return CreateErrorWithDescription("printer_error", error_description);
  }
  return scoped_ptr<base::DictionaryValue>();
}

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessSubmitDoc(
    const GURL& url,
    const net::HttpServerRequestInfo& info,
    net::HttpStatusCode* status_code) const {
  if (!delegate_->IsRegistered() || !delegate_->IsLocalPrintingAllowed()) {
    *status_code = net::HTTP_NOT_FOUND;
    return scoped_ptr<base::DictionaryValue>();
  }

  using net::GetValueForKeyInQuery;

  // Parse query
  LocalPrintJob job;
  std::string offline;
  std::string job_id;
  bool job_name_present = GetValueForKeyInQuery(url, "job_name", &job.job_name);
  bool job_id_present = GetValueForKeyInQuery(url, "job_id", &job_id);
  GetValueForKeyInQuery(url, "client_name", &job.client_name);
  GetValueForKeyInQuery(url, "user_name", &job.user_name);
  GetValueForKeyInQuery(url, "offline", &offline);
  job.offline = (offline == "1");
  job.content_type = info.GetHeaderValue("content-type");
  job.content = info.data;

  // Call delegate
  // TODO(maksymb): Use base::Time for expiration
  int expires_in = 0;
  std::string error_description;
  int timeout;
  LocalPrintJob::SaveResult status = job_id_present
      ? delegate_->SubmitDocWithId(job, job_id, &expires_in,
                                   &error_description, &timeout)
      : delegate_->SubmitDoc(job, &job_id, &expires_in,
                             &error_description, &timeout);

  // Create response
  *status_code = net::HTTP_OK;
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);
  switch (status) {
    case LocalPrintJob::SAVE_SUCCESS:
      response->SetString("job_id", job_id);
      response->SetInteger("expires_in", expires_in);
      response->SetString("job_type", job.content_type);
      response->SetString(
          "job_size",
          base::StringPrintf("%u", static_cast<uint32>(job.content.size())));
      if (job_name_present)
        response->SetString("job_name", job.job_name);
      return response.Pass();

    case LocalPrintJob::SAVE_INVALID_PRINT_JOB:
      return CreateErrorWithTimeout("invalid_print_job", timeout);
    case LocalPrintJob::SAVE_INVALID_DOCUMENT_TYPE:
      return CreateError("invalid_document_type");
    case LocalPrintJob::SAVE_INVALID_DOCUMENT:
      return CreateError("invalid_document");
    case LocalPrintJob::SAVE_DOCUMENT_TOO_LARGE:
      return CreateError("document_too_large");
    case LocalPrintJob::SAVE_PRINTER_BUSY:
      return CreateErrorWithTimeout("printer_busy", -2);
    case LocalPrintJob::SAVE_PRINTER_ERROR:
      return CreateErrorWithDescription("printer_error", error_description);
    default:
      NOTREACHED();
      return scoped_ptr<base::DictionaryValue>();
  }
}

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessJobState(
    const GURL& url,
    net::HttpStatusCode* status_code) const {
  if (!delegate_->IsRegistered() || !delegate_->IsLocalPrintingAllowed()) {
    *status_code = net::HTTP_NOT_FOUND;
    return scoped_ptr<base::DictionaryValue>();
  }

  std::string job_id;
  net::GetValueForKeyInQuery(url, "job_id", &job_id);
  LocalPrintJob::Info info;
  if (!delegate_->GetJobState(job_id, &info))
    return CreateError("invalid_print_job");

  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);
  response->SetString("job_id", job_id);
  response->SetString("state", LocalPrintJobStateToString(info.state));
  response->SetInteger("expires_in", info.expires_in);
  return response.Pass();
}

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessRegister(
    const GURL& url,
    net::HttpStatusCode* status_code) const {
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
  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);

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
    case REG_ERROR_OFFLINE:
      *current_response = CreateError("offline");
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
