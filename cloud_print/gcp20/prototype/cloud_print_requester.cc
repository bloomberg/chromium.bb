// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_requester.h"

#include "base/bind.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

const char kCloudPrintUrl[] = "https://www.google.com/cloudprint";

namespace {

const char kProxyIdValue[] = "proxy";
const char kPrinterNameValue[] = "printer";
const char kPrinterCapsValue[] = "capabilities";
const char kPrinterCapsHashValue[] = "capsHash";
const char kPrinterUserValue[] = "user";

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

}  // namespace

using cloud_print_response_parser::Job;

// Used to return a dummy context, which lives on the message loop
// given in the constructor.
class CloudPrintURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // |task_runner| must not be NULL.
  explicit CloudPrintURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    DCHECK(task_runner);
    network_task_runner_ = task_runner;
  }

  // URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE {
    if (!context_) {
      net::URLRequestContextBuilder builder;
#if defined(OS_LINUX) || defined(OS_ANDROID)
      builder.set_proxy_config_service(
          new net::ProxyConfigServiceFixed(net::ProxyConfig()));
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
      context_.reset(builder.Build());
    }
    return context_.get();
  }

  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE {
    return network_task_runner_;
  }

 protected:
  virtual ~CloudPrintURLRequestContextGetter() {}

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_ptr<net::URLRequestContext> context_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintURLRequestContextGetter);
};

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
  net::AddMultipartFinalDelimiterForUpload(mime_boundary, &data);

  request_ = CreatePost(
      CreateRegisterUrl(),
      data,
      data_mimetype,
      base::Bind(&CloudPrintRequester::ParseRegisterStart, AsWeakPtr()));
  request_->Run(access_token_, context_getter_);
}

void CloudPrintRequester::CompleteRegistration() {
  request_ = CreateGet(
      GURL(polling_url_ + oauth_client_info_.client_id),
      base::Bind(&CloudPrintRequester::ParseRegisterComplete, AsWeakPtr()));
  request_->Run(access_token_, context_getter_);
}

void CloudPrintRequester::FetchPrintJobs(const std::string& refresh_token,
                                         const std::string& device_id) {
  VLOG(3) << "Function: " << __FUNCTION__;
  if (IsBusy())
    return;

  if (access_token_.empty()) {
    UpdateAccesstoken(refresh_token);
    return;
  }

  VLOG(3) << "Function: " << __FUNCTION__ <<
      ": request created";
  request_ = CreateGet(
      CreateFetchUrl(device_id),
      base::Bind(&CloudPrintRequester::ParseFetch, AsWeakPtr()));
  request_->Run(access_token_, context_getter_);
}

void CloudPrintRequester::UpdateAccesstoken(const std::string& refresh_token) {
  VLOG(3) << "Function: " << __FUNCTION__;
  DCHECK(!IsBusy());
  gaia_.reset(new gaia::GaiaOAuthClient(context_getter_.get()));
  gaia_->RefreshToken(oauth_client_info_, refresh_token,
                      std::vector<std::string>(), kGaiaMaxRetries, this);
  delegate_->OnServerError("Access token requested.");
}

void CloudPrintRequester::RequestPrintJob(const Job& job) {
  VLOG(3) << "Function: " << __FUNCTION__;
  current_print_job_.reset(new Job(job));
  request_ = CreateGet(
      CreateControlUrl(current_print_job_->job_id, "IN_PROGRESS"),
      base::Bind(&CloudPrintRequester::ParsePrintJobInProgress, AsWeakPtr()));
  request_->Run(access_token_, context_getter_);
}

void CloudPrintRequester::SendPrintJobDone(const std::string& job_id) {
  VLOG(3) << "Function: " << __FUNCTION__;
  request_ = CreateGet(
      CreateControlUrl(job_id, "DONE"),
      base::Bind(&CloudPrintRequester::ParsePrintJobDone, AsWeakPtr()));
  request_->Run(access_token_, context_getter_);
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
  delegate_->OnServerError("Fetch error");
  NOTIMPLEMENTED();
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
  access_token_ = access_token;
  delegate_->OnGetAuthCodeResponseParsed(refresh_token);
}

void CloudPrintRequester::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();
  access_token_ = access_token;
  LOG(INFO) << "New accesstoken: " << access_token;
}

void CloudPrintRequester::OnOAuthError() {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();
  NOTIMPLEMENTED();
}

void CloudPrintRequester::OnNetworkError(int response_code) {
  VLOG(3) << "Function: " << __FUNCTION__;
  gaia_.reset();
  NOTIMPLEMENTED();
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

  bool success = cloud_print_response_parser::ParseRegisterCompleteResponse(
      response,
      &error_description,
      &authorization_code);

  if (success) {
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
  request_->Run(access_token_, context_getter_);
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
  request_->Run(access_token_, context_getter_);
}

