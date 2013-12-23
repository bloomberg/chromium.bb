// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_requester.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "cloud_print/gcp20/prototype/cloud_print_url_request_context_getter.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

const char kCloudPrintUrl[] = "https://www.google.com/cloudprint";

namespace {

const char kProxyIdValue[] = "proxy";
const char kPrinterNameValue[] = "printer";
const char kPrinterCapsValue[] = "capabilities";
const char kPrinterCapsHashValue[] = "capsHash";
const char kPrinterUserValue[] = "user";
const char kPrinterGcpVersion[] = "gcp_version";
const char kPrinterLocalSettings[] = "local_settings";
const char kPrinterFirmware[] = "firmware";
const char kPrinterManufacturer[] = "manufacturer";
const char kPrinterModel[] = "model";
const char kPrinterSetupUrl[] = "setup_url";
const char kPrinterSupportUrl[] = "support_url";
const char kPrinterUpdateUrl[] = "update_url";

const char kFirmwareValue[] = "2.0";
const char kManufacturerValue[] = "Google";
const char kModelValue[] = "GCPPrototype";

// TODO(maksymb): Replace GCP Version with "2.0" once GCP Server will support it
const char kGcpVersion[] = "1.5";

const int kGaiaMaxRetries = 3;

GURL CreateRegisterUrl() {
  return GURL(std::string(kCloudPrintUrl) + "/register");
}

GURL CreateFetchUrl(const std::string& device_id) {
  GURL url(std::string(kCloudPrintUrl) + "/fetch");
  url = net::AppendQueryParameter(url, "printerid", device_id);
  return url;
}

GURL CreateControlUrl(const std::string& job_id, const std::string& status) {
  GURL url(std::string(kCloudPrintUrl) + "/control");
  url = net::AppendQueryParameter(url, "jobid", job_id);
  url = net::AppendQueryParameter(url, "status", status);
  return url;
}

GURL CreatePrinterUrl(const std::string& device_id) {
  GURL url(std::string(kCloudPrintUrl) + "/printer");
  url = net::AppendQueryParameter(url, "printerid", device_id);
  return url;
}

GURL CreateUpdateUrl(const std::string& device_id) {
  GURL url(std::string(kCloudPrintUrl) + "/update");
  url = net::AppendQueryParameter(url, "printerid", device_id);
  return url;
}

std::string LocalSettingsToJson(const LocalSettings& settings) {
  base::DictionaryValue dictionary;
  scoped_ptr<base::DictionaryValue> current(new base::DictionaryValue);

  // TODO(maksymb): Formalize text as constants.
  current->SetBoolean("local_discovery", settings.local_discovery);
  current->SetBoolean("access_token_enabled", settings.access_token_enabled);
  current->SetBoolean("printer/local_printing_enabled",
                         settings.local_printing_enabled);
  current->SetInteger("xmpp_timeout_value", settings.xmpp_timeout_value);
  dictionary.Set("current", current.release());

  std::string local_settings;
  base::JSONWriter::Write(&dictionary, &local_settings);
  return local_settings;
}

}  // namespace

using cloud_print_response_parser::Job;

CloudPrintRequester::CloudPrintRequester(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    Delegate* delegate)
    : context_getter_(new CloudPrintURLRequestContextGetter(task_runner)),
      delegate_(delegate) {
  oauth_client_info_.client_id =
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_CLOUD_PRINT);
  oauth_client_info_.client_secret =
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_CLOUD_PRINT);
  oauth_client_info_.redirect_uri = "oob";
}

CloudPrintRequester::~CloudPrintRequester() {
}

bool CloudPrintRequester::IsBusy() const {
  return request_ || gaia_;
}

void CloudPrintRequester::StartRegistration(const std::string& proxy_id,
                                            const std::string& device_name,
                                            const std::string& user,
                                            const LocalSettings& settings,
                                            const std::string& cdd) {
  std::string mime_boundary;
  int r1 = base::RandInt(0, kint32max);
  int r2 = base::RandInt(0, kint32max);
  base::SStringPrintf(&mime_boundary,
                      "---------------------------%08X%08X", r1, r2);

  std::string data;
  std::string data_mimetype;
  data_mimetype = "multipart/form-data; boundary=" + mime_boundary;

  net::AddMultipartValueForUpload(kProxyIdValue, proxy_id, mime_boundary,
                                  std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterNameValue, device_name, mime_boundary,
                                  std::string(), &data);
  net::AddMultipartValueForUpload("use_cdd", "true", mime_boundary,
                                  std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterNameValue, device_name, mime_boundary,
                                  std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterCapsValue, cdd, mime_boundary,
                                  "application/json", &data);
  net::AddMultipartValueForUpload(kPrinterCapsHashValue, base::MD5String(cdd),
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterUserValue, user,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterGcpVersion, kGcpVersion,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterLocalSettings,
                                  LocalSettingsToJson(settings),
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterFirmware,
                                  kFirmwareValue,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterManufacturer,
                                  kManufacturerValue,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterModel,
                                  kModelValue,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterSetupUrl,
                                  kCloudPrintUrl,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterSupportUrl,
                                  kCloudPrintUrl,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterUpdateUrl,
                                  kCloudPrintUrl,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartFinalDelimiterForUpload(mime_boundary, &data);

  request_ = CreatePost(
      CreateRegisterUrl(),
      data,
      data_mimetype,
      base::Bind(&CloudPrintRequester::ParseRegisterStart, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::CompleteRegistration() {
  request_ = CreateGet(
      GURL(polling_url_ + oauth_client_info_.client_id),
      base::Bind(&CloudPrintRequester::ParseRegisterComplete, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::FetchPrintJobs(const std::string& device_id) {
  VLOG(3) << "Function: " << __FUNCTION__;
  if (IsBusy())
    return;

  DCHECK(!delegate_->GetAccessToken().empty());

  VLOG(3) << "Function: " << __FUNCTION__ <<
      ": request created";
  request_ = CreateGet(
      CreateFetchUrl(device_id),
      base::Bind(&CloudPrintRequester::ParseFetch, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::UpdateAccesstoken(const std::string& refresh_token) {
  VLOG(3) << "Function: " << __FUNCTION__;
  DCHECK(!IsBusy());
  gaia_.reset(new gaia::GaiaOAuthClient(context_getter_.get()));
  gaia_->RefreshToken(oauth_client_info_, refresh_token,
                      std::vector<std::string>(), kGaiaMaxRetries, this);
}

void CloudPrintRequester::RequestPrintJob(const Job& job) {
  VLOG(3) << "Function: " << __FUNCTION__;
  current_print_job_.reset(new Job(job));
  request_ = CreateGet(
      CreateControlUrl(current_print_job_->job_id, "IN_PROGRESS"),
      base::Bind(&CloudPrintRequester::ParsePrintJobInProgress, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::SendPrintJobDone(const std::string& job_id) {
  VLOG(3) << "Function: " << __FUNCTION__;
  request_ = CreateGet(
      CreateControlUrl(job_id, "DONE"),
      base::Bind(&CloudPrintRequester::ParsePrintJobDone, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::RequestLocalSettings(const std::string& device_id) {
  VLOG(3) << "Function: " << __FUNCTION__;
  request_ = CreateGet(
      CreatePrinterUrl(device_id),
      base::Bind(&CloudPrintRequester::ParseLocalSettings, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::SendLocalSettings(
    const std::string& device_id,
    const LocalSettings& settings) {
  VLOG(3) << "Function: " << __FUNCTION__;

  std::string data_mimetype = "application/x-www-form-urlencoded";
  std::string data = base::StringPrintf(
      "%s=%s",
      kPrinterLocalSettings,
      net::EscapeUrlEncodedData(LocalSettingsToJson(settings), false).c_str());

  request_ = CreatePost(
      CreateUpdateUrl(device_id),
      data, data_mimetype,
      base::Bind(&CloudPrintRequester::ParseLocalSettingUpdated, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}


void CloudPrintRequester::OnFetchComplete(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;
  ParserCallback callback = parser_callback_;
  EraseRequest();
  callback.Run(response);
}

void CloudPrintRequester::OnFetchError(const std::string& server_api,
                                       int server_code,
                                       int server_http_code) {
  VLOG(3) << "Function: " << __FUNCTION__;
  EraseRequest();
  current_print_job_.reset();

  if (server_http_code == net::HTTP_FORBIDDEN) {
    delegate_->OnAuthError();
  } else {
    delegate_->OnServerError("Fetch error");
  }

  // TODO(maksymb): Add Privet |server_http_code| and |server_api| support in
  // case of server errors.
}

void CloudPrintRequester::OnFetchTimeoutReached() {
  VLOG(3) << "Function: " << __FUNCTION__;
  EraseRequest();
  current_print_job_.reset();
  delegate_->OnNetworkError();
}

void CloudPrintRequester::OnGetTokensResponse(const std::string& refresh_token,
                                              const std::string& access_token,
                                              int expires_in_seconds) {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();
  delegate_->OnRegistrationFinished(refresh_token,
                                    access_token, expires_in_seconds);
}

void CloudPrintRequester::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();
  delegate_->OnAccesstokenReceviced(access_token, expires_in_seconds);
}

void CloudPrintRequester::OnOAuthError() {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();
  delegate_->OnAuthError();
}

void CloudPrintRequester::OnNetworkError(int response_code) {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();

  if (response_code == net::HTTP_FORBIDDEN) {
    // TODO(maksymb): delegate_->OnPrinterDeleted();
  } else {
    delegate_->OnNetworkError();
  }
}

scoped_ptr<CloudPrintRequest> CloudPrintRequester::CreateGet(
    const GURL& url,
    const ParserCallback& parser_callback) {
  DCHECK(!IsBusy());
  DCHECK(parser_callback_.is_null());
  parser_callback_ = parser_callback;
  return CloudPrintRequest::CreateGet(url, this);
}

scoped_ptr<CloudPrintRequest> CloudPrintRequester::CreatePost(
    const GURL& url,
    const std::string& content,
    const std::string& mimetype,
    const ParserCallback& parser_callback) {
  DCHECK(!IsBusy());
  DCHECK(parser_callback_.is_null());
  parser_callback_ = parser_callback;
  return CloudPrintRequest::CreatePost(url, content, mimetype, this);
}

void CloudPrintRequester::EraseRequest() {
  DCHECK(request_);
  DCHECK(!parser_callback_.is_null());
  request_.reset();
  parser_callback_.Reset();
}

void CloudPrintRequester::ParseRegisterStart(const std::string& response) {
  std::string error_description;
  std::string polling_url;
  std::string registration_token;
  std::string complete_invite_url;
  std::string device_id;

  bool success = cloud_print_response_parser::ParseRegisterStartResponse(
      response,
      &error_description,
      &polling_url,
      &registration_token,
      &complete_invite_url,
      &device_id);

  if (success) {
    polling_url_ = polling_url;
    delegate_->OnRegistrationStartResponseParsed(registration_token,
                                                 complete_invite_url,
                                                 device_id);
  } else {
    delegate_->OnRegistrationError(error_description);
  }
}

void CloudPrintRequester::ParseRegisterComplete(const std::string& response) {
  std::string error_description;
  std::string authorization_code;

  std::string xmpp_jid;
  bool success = cloud_print_response_parser::ParseRegisterCompleteResponse(
      response,
      &error_description,
      &authorization_code,
      &xmpp_jid);

  if (success) {
    delegate_->OnXmppJidReceived(xmpp_jid);

    gaia_.reset(new gaia::GaiaOAuthClient(context_getter_.get()));
    gaia_->GetTokensFromAuthCode(oauth_client_info_, authorization_code,
                                 kGaiaMaxRetries, this);
  } else {
    delegate_->OnRegistrationError(error_description);
  }
}

void CloudPrintRequester::ParseFetch(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;

  std::string error_description;
  std::vector<Job> list;
  bool success = cloud_print_response_parser::ParseFetchResponse(
      response,
      &error_description,
      &list);

  if (success) {
    delegate_->OnPrintJobsAvailable(list);
  } else {
    delegate_->OnServerError(error_description);
  }
}

void CloudPrintRequester::ParseGetPrintJobTicket(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;
  current_print_job_->ticket = response;

  DCHECK(current_print_job_);
  request_ = CreateGet(
      GURL(current_print_job_->file_url),
      base::Bind(&CloudPrintRequester::ParseGetPrintJobData, AsWeakPtr()));
  request_->AddHeader("Accept: \"application/pdf\"");
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::ParseGetPrintJobData(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;
  current_print_job_->file = response;
  DCHECK(current_print_job_);
  delegate_->OnPrintJobDownloaded(*current_print_job_);
}

void CloudPrintRequester::ParsePrintJobDone(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;
  current_print_job_.reset();
  delegate_->OnPrintJobDone();
}

void CloudPrintRequester::ParsePrintJobInProgress(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;
  DCHECK(current_print_job_);
  request_ = CreateGet(
      GURL(current_print_job_->ticket_url),
      base::Bind(&CloudPrintRequester::ParseGetPrintJobTicket, AsWeakPtr()));
  request_->Run(delegate_->GetAccessToken(), context_getter_);
}

void CloudPrintRequester::ParseLocalSettings(const std::string& response) {
  VLOG(3) << "Function: " << __FUNCTION__;

  std::string error_description;
  LocalSettings settings;
  LocalSettings::State state;

  bool success = cloud_print_response_parser::ParseLocalSettingsResponse(
      response,
      &error_description,
      &state,
      &settings);

  if (success) {
    delegate_->OnLocalSettingsReceived(state, settings);
  } else {
    delegate_->OnServerError(error_description);
  }
}

void CloudPrintRequester::ParseLocalSettingUpdated(
    const std::string& response) {
  delegate_->OnLocalSettingsUpdated();
}
