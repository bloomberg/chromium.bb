// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace local_discovery {

namespace {
const char kUrlPlaceHolder[] = "http://host/";
const char kPrivetRegisterActionArgName[] = "action";
const char kPrivetRegisterUserArgName[] = "user";

const char kPrivetURLKeyUser[] = "user";
const char kPrivetURLKeyJobname[] = "jobname";
const char kPrivetURLKeyOffline[] = "offline";
const char kPrivetURLValueOffline[] = "1";

const char kPrivetContentTypePDF[] = "application/pdf";
const char kPrivetContentTypePWGRaster[] = "image/pwg-raster";
const char kPrivetContentTypeAny[] = "*/*";
const char kPrivetContentTypeCJT[] = "application/json";

const char kPrivetCDDKeySupportedContentTypes[] =
    "printer.supported_content_type";

const char kPrivetCDDKeyContentType[] = "content_type";

const char kPrivetKeyJobID[] = "job_id";

const int kPrivetCancelationTimeoutSeconds = 3;

const int kPrivetLocalPrintMaxRetries = 2;

const int kPrivetLocalPrintDefaultTimeout = 5;

GURL CreatePrivetURL(const std::string& path) {
  GURL url(kUrlPlaceHolder);
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return url.ReplaceComponents(replacements);
}

GURL CreatePrivetRegisterURL(const std::string& action,
                             const std::string& user) {
  GURL url = CreatePrivetURL(kPrivetRegisterPath);
  url = net::AppendQueryParameter(url, kPrivetRegisterActionArgName, action);
  return net::AppendQueryParameter(url, kPrivetRegisterUserArgName, user);
}

}  // namespace

PrivetInfoOperationImpl::PrivetInfoOperationImpl(
    PrivetHTTPClientImpl* privet_client,
    PrivetInfoOperation::Delegate* delegate)
    : privet_client_(privet_client), delegate_(delegate) {
}

PrivetInfoOperationImpl::~PrivetInfoOperationImpl() {
}

void PrivetInfoOperationImpl::Start() {
  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetURL(kPrivetInfoPath), net::URLFetcher::GET, this);

  url_fetcher_->DoNotRetryOnTransientError();
  url_fetcher_->AllowEmptyPrivetToken();

  url_fetcher_->Start();
}

PrivetHTTPClient* PrivetInfoOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetInfoOperationImpl::OnError(PrivetURLFetcher* fetcher,
                                      PrivetURLFetcher::ErrorType error) {
  if (error == PrivetURLFetcher::RESPONSE_CODE_ERROR) {
    delegate_->OnPrivetInfoDone(this, fetcher->response_code(), NULL);
  } else {
    delegate_->OnPrivetInfoDone(this, kPrivetHTTPCodeInternalFailure, NULL);
  }
}

void PrivetInfoOperationImpl::OnParsedJson(PrivetURLFetcher* fetcher,
                                           const base::DictionaryValue* value,
                                           bool has_error) {
  if (!has_error)
    privet_client_->CacheInfo(value);
  delegate_->OnPrivetInfoDone(this, fetcher->response_code(), value);
}

PrivetRegisterOperationImpl::PrivetRegisterOperationImpl(
    PrivetHTTPClientImpl* privet_client,
    const std::string& user,
    PrivetRegisterOperation::Delegate* delegate)
    : user_(user), delegate_(delegate), privet_client_(privet_client),
      ongoing_(false) {
}

PrivetRegisterOperationImpl::~PrivetRegisterOperationImpl() {
}

void PrivetRegisterOperationImpl::Start() {
  ongoing_ = true;
  next_response_handler_ =
      base::Bind(&PrivetRegisterOperationImpl::StartResponse,
                 base::Unretained(this));
  SendRequest(kPrivetActionStart);
}

void PrivetRegisterOperationImpl::Cancel() {
  url_fetcher_.reset();

  if (ongoing_) {
    // Owned by the message loop.
    Cancelation* cancelation = new Cancelation(privet_client_, user_);

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PrivetRegisterOperationImpl::Cancelation::Cleanup,
                   base::Owned(cancelation)),
        base::TimeDelta::FromSeconds(kPrivetCancelationTimeoutSeconds));

    ongoing_ = false;
  }
}

void PrivetRegisterOperationImpl::CompleteRegistration() {
  next_response_handler_ =
      base::Bind(&PrivetRegisterOperationImpl::CompleteResponse,
                 base::Unretained(this));
  SendRequest(kPrivetActionComplete);
}

PrivetHTTPClient* PrivetRegisterOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetRegisterOperationImpl::OnError(PrivetURLFetcher* fetcher,
                                          PrivetURLFetcher::ErrorType error) {
  ongoing_ = false;
  int visible_http_code = -1;
  FailureReason reason = FAILURE_NETWORK;

  if (error == PrivetURLFetcher::RESPONSE_CODE_ERROR) {
    visible_http_code = fetcher->response_code();
    reason = FAILURE_HTTP_ERROR;
  } else if (error == PrivetURLFetcher::JSON_PARSE_ERROR) {
    reason = FAILURE_MALFORMED_RESPONSE;
  } else if (error == PrivetURLFetcher::TOKEN_ERROR) {
    reason = FAILURE_TOKEN;
  } else if (error == PrivetURLFetcher::RETRY_ERROR) {
    reason = FAILURE_RETRY;
  }

  delegate_->OnPrivetRegisterError(this,
                                   current_action_,
                                   reason,
                                   visible_http_code,
                                   NULL);
}

void PrivetRegisterOperationImpl::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue* value,
    bool has_error) {
  if (has_error) {
    std::string error;
    value->GetString(kPrivetKeyError, &error);

    ongoing_ = false;
    delegate_->OnPrivetRegisterError(this,
                                     current_action_,
                                     FAILURE_JSON_ERROR,
                                     fetcher->response_code(),
                                     value);
    return;
  }

  // TODO(noamsml): Match the user&action with the user&action in the object,
  // and fail if different.

  next_response_handler_.Run(*value);
}

void PrivetRegisterOperationImpl::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  privet_client_->RefreshPrivetToken(callback);
}

void PrivetRegisterOperationImpl::SendRequest(const std::string& action) {
  current_action_ = action;
  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetRegisterURL(action, user_), net::URLFetcher::POST, this);
  url_fetcher_->Start();
}

void PrivetRegisterOperationImpl::StartResponse(
    const base::DictionaryValue& value) {
  next_response_handler_ =
      base::Bind(&PrivetRegisterOperationImpl::GetClaimTokenResponse,
                 base::Unretained(this));

  SendRequest(kPrivetActionGetClaimToken);
}

void PrivetRegisterOperationImpl::GetClaimTokenResponse(
    const base::DictionaryValue& value) {
  std::string claimUrl;
  std::string claimToken;
  bool got_url = value.GetString(kPrivetKeyClaimURL, &claimUrl);
  bool got_token = value.GetString(kPrivetKeyClaimToken, &claimToken);
  if (got_url || got_token) {
    delegate_->OnPrivetRegisterClaimToken(this, claimToken, GURL(claimUrl));
  } else {
    delegate_->OnPrivetRegisterError(this,
                                     current_action_,
                                     FAILURE_MALFORMED_RESPONSE,
                                     -1,
                                     NULL);
  }
}

void PrivetRegisterOperationImpl::CompleteResponse(
    const base::DictionaryValue& value) {
  std::string id;
  value.GetString(kPrivetKeyDeviceID, &id);
  ongoing_ = false;
  expected_id_ = id;
  StartInfoOperation();
}

void PrivetRegisterOperationImpl::OnPrivetInfoDone(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* value) {
  // TODO(noamsml): Simplify error case.
  if (!value) {
    delegate_->OnPrivetRegisterError(this,
                                     kPrivetActionNameInfo,
                                     FAILURE_NETWORK,
                                     -1,
                                     NULL);
    return;
  }

  if (!value->HasKey(kPrivetInfoKeyID)) {
    if (value->HasKey(kPrivetKeyError)) {
      delegate_->OnPrivetRegisterError(this,
                                       kPrivetActionNameInfo,
                                        FAILURE_JSON_ERROR,
                                       http_code,
                                       value);
    } else {
      delegate_->OnPrivetRegisterError(this,
                                       kPrivetActionNameInfo,
                                       FAILURE_MALFORMED_RESPONSE,
                                       -1,
                                       NULL);
    }
    return;
  }

  std::string id;

  if (!value->GetString(kPrivetInfoKeyID, &id) ||
      id != expected_id_) {
    delegate_->OnPrivetRegisterError(this,
                                     kPrivetActionNameInfo,
                                     FAILURE_MALFORMED_RESPONSE,
                                     -1,
                                     NULL);
  } else {
    delegate_->OnPrivetRegisterDone(this, id);
  }
}

void PrivetRegisterOperationImpl::StartInfoOperation() {
  info_operation_ = privet_client_->CreateInfoOperation(this);
  info_operation_->Start();
}

PrivetRegisterOperationImpl::Cancelation::Cancelation(
    PrivetHTTPClientImpl* privet_client,
    const std::string& user) {
  url_fetcher_ =
      privet_client->CreateURLFetcher(
          CreatePrivetRegisterURL(kPrivetActionCancel, user),
          net::URLFetcher::POST, this);
  url_fetcher_->DoNotRetryOnTransientError();
  url_fetcher_->Start();
}

PrivetRegisterOperationImpl::Cancelation::~Cancelation() {
}

void PrivetRegisterOperationImpl::Cancelation::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
}

void PrivetRegisterOperationImpl::Cancelation::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue* value,
    bool has_error) {
}

void PrivetRegisterOperationImpl::Cancelation::Cleanup() {
  // Nothing needs to be done, as base::Owned will delete this object,
  // this callback is just here to pass ownership of the Cancelation to
  // the message loop.
}

PrivetCapabilitiesOperationImpl::PrivetCapabilitiesOperationImpl(
    PrivetHTTPClientImpl* privet_client,
    PrivetCapabilitiesOperation::Delegate* delegate)
    : privet_client_(privet_client), delegate_(delegate) {
}

PrivetCapabilitiesOperationImpl::~PrivetCapabilitiesOperationImpl() {
}

void PrivetCapabilitiesOperationImpl::Start() {
  url_fetcher_ = privet_client_->CreateURLFetcher(
      CreatePrivetURL(kPrivetCapabilitiesPath), net::URLFetcher::GET, this);
  url_fetcher_->DoNotRetryOnTransientError();
  url_fetcher_->Start();
}

PrivetHTTPClient* PrivetCapabilitiesOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetCapabilitiesOperationImpl::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  delegate_->OnPrivetCapabilities(this, -1, NULL);
}

void PrivetCapabilitiesOperationImpl::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue* value,
    bool has_error) {
  delegate_->OnPrivetCapabilities(this, fetcher->response_code(), value);
}

void PrivetCapabilitiesOperationImpl::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  privet_client_->RefreshPrivetToken(callback);
}

PrivetLocalPrintOperationImpl::PrivetLocalPrintOperationImpl(
    PrivetHTTPClientImpl* privet_client,
    PrivetLocalPrintOperation::Delegate* delegate)
    : privet_client_(privet_client), delegate_(delegate),
      use_pdf_(false), has_capabilities_(false), has_extended_workflow_(false),
      started_(false), offline_(false), invalid_job_retries_(0),
      weak_factory_(this) {
}

PrivetLocalPrintOperationImpl::~PrivetLocalPrintOperationImpl() {
}

void PrivetLocalPrintOperationImpl::Start() {
  DCHECK(!started_);

  // We need to get the /info response so we can know which APIs are available.
  // TODO(noamsml): Use cached info when available.
  info_operation_ = privet_client_->CreateInfoOperation(this);
  info_operation_->Start();

  started_ = true;
}

void PrivetLocalPrintOperationImpl::OnPrivetInfoDone(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* value) {
  if (value && !value->HasKey(kPrivetKeyError)) {
    has_capabilities_ = false;
    has_extended_workflow_ = false;
    bool has_printing = false;

    const base::ListValue* api_list;
    if (value->GetList(kPrivetInfoKeyAPIList, &api_list)) {
      for (size_t i = 0; i < api_list->GetSize(); i++) {
        std::string api;
        api_list->GetString(i, &api);
        if (api == kPrivetCapabilitiesPath) {
          has_capabilities_ = true;
        } else if (api == kPrivetSubmitdocPath) {
          has_printing = true;
        } else if (api == kPrivetCreatejobPath) {
          has_extended_workflow_ = true;
        }
      }
    }

    if (!has_printing) {
      delegate_->OnPrivetPrintingError(this, -1);
      return;
    }

    StartInitialRequest();
  } else {
    delegate_->OnPrivetPrintingError(this, http_code);
  }
}

void PrivetLocalPrintOperationImpl::StartInitialRequest() {
  if (has_capabilities_) {
    GetCapabilities();
  } else {
    // Since we have no capabiltties, the only reasonable format we can
    // request is PWG Raster.
    use_pdf_ = false;
    delegate_->OnPrivetPrintingRequestPWGRaster(this);
  }
}

void PrivetLocalPrintOperationImpl::GetCapabilities() {
  current_response_ = base::Bind(
      &PrivetLocalPrintOperationImpl::OnCapabilitiesResponse,
      base::Unretained(this));

  url_fetcher_= privet_client_->CreateURLFetcher(
      CreatePrivetURL(kPrivetCapabilitiesPath), net::URLFetcher::GET, this);
  url_fetcher_->DoNotRetryOnTransientError();

  url_fetcher_->Start();
}

void PrivetLocalPrintOperationImpl::DoCreatejob() {
  current_response_ = base::Bind(
      &PrivetLocalPrintOperationImpl::OnCreatejobResponse,
      base::Unretained(this));

  url_fetcher_= privet_client_->CreateURLFetcher(
      CreatePrivetURL(kPrivetCreatejobPath), net::URLFetcher::POST, this);
  url_fetcher_->SetUploadData(kPrivetContentTypeCJT, ticket_);

  url_fetcher_->Start();
}

void PrivetLocalPrintOperationImpl::DoSubmitdoc() {
  current_response_ = base::Bind(
      &PrivetLocalPrintOperationImpl::OnSubmitdocResponse,
      base::Unretained(this));

  GURL url = CreatePrivetURL(kPrivetSubmitdocPath);

  if (!user_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetURLKeyUser,
                                    user_);
  }

  if (!jobname_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetURLKeyJobname,
                                    jobname_);
  }

  if (!jobid_.empty()) {
    url = net::AppendQueryParameter(url,
                                    kPrivetKeyJobID,
                                    jobid_);
  }

  if (offline_) {
    url = net::AppendQueryParameter(url,
                                    kPrivetURLKeyOffline,
                                    kPrivetURLValueOffline);
  }

  url_fetcher_= privet_client_->CreateURLFetcher(
      url, net::URLFetcher::POST, this);

  DCHECK(!data_.empty());
  url_fetcher_->SetUploadData(
      use_pdf_ ? kPrivetContentTypePDF : kPrivetContentTypePWGRaster,
      data_);

  url_fetcher_->Start();
}

void PrivetLocalPrintOperationImpl::OnCapabilitiesResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  if (has_error) {
    delegate_->OnPrivetPrintingError(this, 200);
    return;
  }

  const base::ListValue* supported_content_types;
  use_pdf_ = false;

  if (value->GetList(kPrivetCDDKeySupportedContentTypes,
                     &supported_content_types)) {
    for (size_t i = 0; i < supported_content_types->GetSize();
         i++) {
      const base::DictionaryValue* content_type_value;
      std::string content_type;

      if (supported_content_types->GetDictionary(i, &content_type_value) &&
          content_type_value->GetString(kPrivetCDDKeyContentType,
                                        &content_type) &&
          (content_type == kPrivetContentTypePDF ||
           content_type == kPrivetContentTypeAny) ) {
        use_pdf_ = true;
      }
    }
  }

  if (use_pdf_)
    delegate_->OnPrivetPrintingRequestPDF(this);
  else
    delegate_->OnPrivetPrintingRequestPWGRaster(this);
}

void PrivetLocalPrintOperationImpl::OnSubmitdocResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  std::string error;
  // This error is only relevant in the case of extended workflow:
  // If the print job ID is invalid, retry createjob and submitdoc,
  // rather than simply retrying the current request.
  if (has_error) {
    if (has_extended_workflow_ &&
        value->GetString(kPrivetKeyError, &error) &&
        error == kPrivetErrorInvalidPrintJob &&
        invalid_job_retries_ < kPrivetLocalPrintMaxRetries) {
      invalid_job_retries_++;

      int timeout = kPrivetLocalPrintDefaultTimeout;
      value->GetInteger(kPrivetKeyTimeout, &timeout);

      double random_scaling_factor =
          1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

      timeout = static_cast<int>(timeout * random_scaling_factor);

      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE, base::Bind(&PrivetLocalPrintOperationImpl::DoCreatejob,
                                weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromSeconds(timeout));
    } else {
      delegate_->OnPrivetPrintingError(this, 200);
    }

    return;
  }


  // If we've gotten this far, there are no errors, so we've effectively
  // succeeded.
  delegate_->OnPrivetPrintingDone(this);
}

void PrivetLocalPrintOperationImpl::OnCreatejobResponse(
    bool has_error,
    const base::DictionaryValue* value) {
  if (has_error) {
    delegate_->OnPrivetPrintingError(this, 200);
    return;
  }

  // Try to get job ID from value. If not, jobid_ will be empty and we will use
  // simple printing.
  value->GetString(kPrivetKeyJobID, &jobid_);

  DoSubmitdoc();
}

PrivetHTTPClient* PrivetLocalPrintOperationImpl::GetHTTPClient() {
  return privet_client_;
}

void PrivetLocalPrintOperationImpl::OnError(
    PrivetURLFetcher* fetcher,
    PrivetURLFetcher::ErrorType error) {
  delegate_->OnPrivetPrintingError(this, -1);
}

void PrivetLocalPrintOperationImpl::OnParsedJson(
    PrivetURLFetcher* fetcher,
    const base::DictionaryValue* value,
    bool has_error) {
  DCHECK(!current_response_.is_null());
  current_response_.Run(has_error, value);
}

void PrivetLocalPrintOperationImpl::OnNeedPrivetToken(
    PrivetURLFetcher* fetcher,
    const PrivetURLFetcher::TokenCallback& callback) {
  privet_client_->RefreshPrivetToken(callback);
}

void PrivetLocalPrintOperationImpl::SendData(const std::string& data) {
  DCHECK(started_);
  data_ = data;

  if (has_extended_workflow_ && !ticket_.empty()) {
    DoCreatejob();
  } else {
    DoSubmitdoc();
  }
}

void PrivetLocalPrintOperationImpl::SetTicket(const std::string& ticket) {
  DCHECK(!started_);
  ticket_ = ticket;
}

void PrivetLocalPrintOperationImpl::SetUsername(const std::string& user) {
  DCHECK(!started_);
  user_= user;
}

void PrivetLocalPrintOperationImpl::SetJobname(const std::string& jobname) {
  DCHECK(!started_);
  jobname_ = jobname;
}

void PrivetLocalPrintOperationImpl::SetOffline(bool offline) {
  DCHECK(!started_);
  offline_ = offline;
}

PrivetHTTPClientImpl::PrivetHTTPClientImpl(
    const std::string& name,
    const net::HostPortPair& host_port,
    net::URLRequestContextGetter* request_context)
    : name_(name),
      fetcher_factory_(request_context),
      host_port_(host_port) {
}

PrivetHTTPClientImpl::~PrivetHTTPClientImpl() {
}

const base::DictionaryValue* PrivetHTTPClientImpl::GetCachedInfo() const {
  return cached_info_.get();
}

scoped_ptr<PrivetRegisterOperation>
PrivetHTTPClientImpl::CreateRegisterOperation(
    const std::string& user,
    PrivetRegisterOperation::Delegate* delegate) {
  return scoped_ptr<PrivetRegisterOperation>(
      new PrivetRegisterOperationImpl(this, user, delegate));
}

scoped_ptr<PrivetInfoOperation> PrivetHTTPClientImpl::CreateInfoOperation(
    PrivetInfoOperation::Delegate* delegate) {
  return scoped_ptr<PrivetInfoOperation>(
      new PrivetInfoOperationImpl(this, delegate));
}

scoped_ptr<PrivetCapabilitiesOperation>
PrivetHTTPClientImpl::CreateCapabilitiesOperation(
    PrivetCapabilitiesOperation::Delegate* delegate) {
  return scoped_ptr<PrivetCapabilitiesOperation>(
      new PrivetCapabilitiesOperationImpl(this, delegate));
}

scoped_ptr<PrivetLocalPrintOperation>
PrivetHTTPClientImpl::CreateLocalPrintOperation(
    PrivetLocalPrintOperation::Delegate* delegate) {
  return scoped_ptr<PrivetLocalPrintOperation>(
      new PrivetLocalPrintOperationImpl(this, delegate));
}

const std::string& PrivetHTTPClientImpl::GetName() {
  return name_;
}

scoped_ptr<PrivetURLFetcher> PrivetHTTPClientImpl::CreateURLFetcher(
    const GURL& url, net::URLFetcher::RequestType request_type,
    PrivetURLFetcher::Delegate* delegate) const {
  GURL::Replacements replacements;
  replacements.SetHostStr(host_port_.host());
  std::string port(base::IntToString(host_port_.port()));  // Keep string alive.
  replacements.SetPortStr(port);
  GURL url2 = url.ReplaceComponents(replacements);
  return fetcher_factory_.CreateURLFetcher(url.ReplaceComponents(replacements),
                                           request_type, delegate);
}

void PrivetHTTPClientImpl::CacheInfo(const base::DictionaryValue* cached_info) {
  cached_info_.reset(cached_info->DeepCopy());
  std::string token;
  if (cached_info_->GetString(kPrivetInfoKeyToken, &token)) {
    fetcher_factory_.set_token(token);
  }
}

bool PrivetHTTPClientImpl::HasToken() const {
  return fetcher_factory_.get_token() != "";
};

void PrivetHTTPClientImpl::RefreshPrivetToken(
    const PrivetURLFetcher::TokenCallback& callback) {
  token_callbacks_.push_back(callback);

  if (!info_operation_) {
    info_operation_ = CreateInfoOperation(this);
    info_operation_->Start();
  }
}

void PrivetHTTPClientImpl::OnPrivetInfoDone(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* value) {
  info_operation_.reset();
  std::string token;

  // If this does not succeed, token will be empty, and an empty string
  // is our sentinel value, since empty X-Privet-Tokens are not allowed.
  if (value) {
    value->GetString(kPrivetInfoKeyToken, &token);
  }

  TokenCallbackVector token_callbacks;
  token_callbacks_.swap(token_callbacks);

  for (TokenCallbackVector::iterator i = token_callbacks.begin();
       i != token_callbacks.end(); i++) {
    i->Run(token);
  }
}

}  // namespace local_discovery
