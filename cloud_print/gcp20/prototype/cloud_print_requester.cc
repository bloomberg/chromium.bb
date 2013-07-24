// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_requester.h"

#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "cloud_print/gcp20/prototype/cloud_print_response_parser.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
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

}  // namespace

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

bool CloudPrintRequester::StartRegistration(const std::string& proxy_id,
                                            const std::string& device_name,
                                            const std::string& user,
                                            const std::string& cdd) {
  std::string mime_boundary;
  int r1 = base::RandInt(0, kint32max);
  int r2 = base::RandInt(0, kint32max);
  base::SStringPrintf(&mime_boundary,
                      "---------------------------%08X%08X", r1, r2);

  std::string data;
  net::AddMultipartValueForUpload(kProxyIdValue, proxy_id,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterNameValue, device_name,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload("use_cdd", "true",
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterNameValue, device_name,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterCapsValue, cdd,
                                  mime_boundary, "application/json", &data);
  net::AddMultipartValueForUpload(kPrinterCapsHashValue, base::MD5String(cdd),
                                  mime_boundary, std::string(), &data);
  net::AddMultipartValueForUpload(kPrinterUserValue, user,
                                  mime_boundary, std::string(), &data);
  net::AddMultipartFinalDelimiterForUpload(mime_boundary, &data);

  std::string mime_type("multipart/form-data; boundary=" + mime_boundary);

  if (!CreateRequest(
      CreateRegisterUrl(), net::URLFetcher::POST,
      base::Bind(&CloudPrintRequester::ParseRegisterStartResponse,
                 AsWeakPtr()))) {
    return false;
  }
  fetcher_->SetUploadData(mime_type, data);
  fetcher_->Start();

  return true;
}

bool CloudPrintRequester::CompleteRegistration() {
  if (!CreateRequest(
      GURL(polling_url_ + oauth_client_info_.client_id),
      net::URLFetcher::GET,
      base::Bind(&CloudPrintRequester::ParseRegisterCompleteResponse,
                 AsWeakPtr()))) {
    return false;
  }
  fetcher_->Start();

  return true;
}

void CloudPrintRequester::OnURLFetchComplete(const net::URLFetcher* source) {
  std::string response;
  source->GetResponseAsString(&response);
  fetcher_.reset();
  VLOG(1) << response;

  // TODO(maksymb): Add |server_http_code| and |server_api| info for errors.
  if (!parse_response_callback_.is_null()) {
    parse_response_callback_.Run(response);
    parse_response_callback_.Reset();
  }
}

void CloudPrintRequester::OnGetTokensResponse(const std::string& refresh_token,
                                              const std::string& access_token,
                                              int expires_in_seconds) {
  access_token_ = access_token;
  delegate_->OnGetAuthCodeResponseParsed(refresh_token);
}

void CloudPrintRequester::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  NOTIMPLEMENTED();
}

void CloudPrintRequester::OnOAuthError() {
  NOTIMPLEMENTED();
}

void CloudPrintRequester::OnNetworkError(int response_code) {
  NOTIMPLEMENTED();
}

bool CloudPrintRequester::CreateRequest(const GURL& url,
                                        net::URLFetcher::RequestType method,
                                        const ParserCallback& callback) {
  if (fetcher_)
    return false;  // Only one query is supported.

  fetcher_.reset(net::URLFetcher::Create(url, method, this));
  fetcher_->SetRequestContext(context_getter_);

  int load_flags = fetcher_->GetLoadFlags();
  load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  fetcher_->SetLoadFlags(load_flags);

  fetcher_->AddExtraRequestHeader("X-CloudPrint-Proxy: \"\"");
  if (!access_token_.empty())
    fetcher_->AddExtraRequestHeader("Authorization: Bearer " + access_token_);

  parse_response_callback_ = callback;

  return true;
}

void CloudPrintRequester::ParseRegisterStartResponse(
    const std::string& response) {
  std::string polling_url;
  std::string registration_token;
  std::string complete_invite_url;
  std::string device_id;

  bool success = cloud_print_response_parser::ParseRegisterStartResponse(
      response,
      base::Bind(&CloudPrintRequester::Delegate::OnRegistrationError,
                 base::Unretained(delegate_)),
      &polling_url,
      &registration_token,
      &complete_invite_url,
      &device_id);

  if (!success)
    return;

  polling_url_ = polling_url;
  delegate_->OnRegistrationStartResponseParsed(registration_token,
                                               complete_invite_url, device_id);
}

void CloudPrintRequester::ParseRegisterCompleteResponse(
    const std::string& response) {
  std::string authorization_code;

  bool success = cloud_print_response_parser::ParseRegisterCompleteResponse(
      response,
      base::Bind(&CloudPrintRequester::Delegate::OnRegistrationError,
                 base::Unretained(delegate_)),
      &authorization_code);

  if (!success)
    return;

  gaia_.reset(new gaia::GaiaOAuthClient(context_getter_.get()));
  gaia_->GetTokensFromAuthCode(oauth_client_info_, authorization_code,
                               kGaiaMaxRetries, this);
}

