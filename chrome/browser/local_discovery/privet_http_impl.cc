// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/privet_constants.h"
#include "url/gurl.h"

namespace local_discovery {

namespace {
// First format argument (string) is the host, second format argument (int) is
// the port.
const char kPrivetInfoURLFormat[] = "http://%s:%d/privet/info";
// First format argument (string) is the host, second format argument (int) is
// the port, third argument (string) is the action name, fourth argument
// (string) is the user name.
const char kPrivetRegisterURLFormat[] =
    "http://%s:%d/privet/register?action=%s&user=%s";

const int kPrivetCancelationTimeoutSeconds = 3;
}  // namespace

PrivetInfoOperationImpl::PrivetInfoOperationImpl(
    PrivetHTTPClientImpl* privet_client,
    PrivetInfoOperation::Delegate* delegate)
    : privet_client_(privet_client), delegate_(delegate) {
}

PrivetInfoOperationImpl::~PrivetInfoOperationImpl() {
}

void PrivetInfoOperationImpl::Start() {
  std::string url = base::StringPrintf(
      kPrivetInfoURLFormat,
      privet_client_->host_port().host().c_str(),
      privet_client_->host_port().port());

  url_fetcher_ = privet_client_->fetcher_factory().CreateURLFetcher(
      GURL(url), net::URLFetcher::GET, this);

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
      ongoing_(false), info_for_confirmation_(false) {
}

PrivetRegisterOperationImpl::~PrivetRegisterOperationImpl() {
}

void PrivetRegisterOperationImpl::Start() {
  if (privet_client_->fetcher_factory().get_token() == "") {
    StartInfoOperation();
    return;
  }

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

    if (error == kPrivetErrorInvalidXPrivetToken) {
      StartInfoOperation();

      // Use a list of transient error names, but also detect if a "timeout"
      // key is present as a fallback.
    } else if (PrivetErrorTransient(error) ||
               value->HasKey(kPrivetKeyTimeout)) {
      int timeout_seconds;
      double random_scaling_factor =
          1 + base::RandDouble() * kPrivetMaximumTimeRandomAddition;

      if (!value->GetInteger(kPrivetKeyTimeout, &timeout_seconds)) {
        timeout_seconds = kPrivetDefaultTimeout;
      }

      int timeout_seconds_randomized =
          static_cast<int>(timeout_seconds * random_scaling_factor);

      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&PrivetRegisterOperationImpl::SendRequest,
                     AsWeakPtr(), current_action_),
                     base::TimeDelta::FromSeconds(timeout_seconds_randomized));
    } else  {
      ongoing_ = false;
      delegate_->OnPrivetRegisterError(this,
                                       current_action_,
                                       FAILURE_JSON_ERROR,
                                       fetcher->response_code(),
                                       value);
    }

    return;
  }

  // TODO(noamsml): Match the user&action with the user&action in the object,
  // and fail if different.

  next_response_handler_.Run(*value);
}

void PrivetRegisterOperationImpl::SendRequest(const std::string& action) {
  GURL url = GetURLForActionAndUser(privet_client_, action, user_);

  current_action_ = action;
  url_fetcher_ = privet_client_->fetcher_factory().CreateURLFetcher(
      url, net::URLFetcher::POST, this);
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
  info_for_confirmation_ = true;
  StartInfoOperation();
}

void PrivetRegisterOperationImpl::OnPrivetInfoDone(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* value) {
  if (!info_for_confirmation_) {
    GetTokenFromInfoCall(operation, http_code, value);
  } else {
    VerifyIDFromInfoCall(operation, http_code, value);
  }
}

void PrivetRegisterOperationImpl::GetTokenFromInfoCall(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* value) {
  // TODO(noamsml): Distinguish between network errors and unparsable JSON in
  // this case.
  if (!value) {
    delegate_->OnPrivetRegisterError(this,
                                     kPrivetActionNameInfo,
                                     FAILURE_NETWORK,
                                     -1,
                                     NULL);
    return;
  }

  // If there is a key in the info response, the InfoOperation
  // has stored it in the client.
  if (!value->HasKey(kPrivetInfoKeyToken)) {
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

  if (!ongoing_) {
    Start();
  } else {
    SendRequest(current_action_);
  }
}

void PrivetRegisterOperationImpl::VerifyIDFromInfoCall(
    PrivetInfoOperation* operation,
    int http_code,
    const base::DictionaryValue* value) {
  // TODO (noamsml): Simplify error case.
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

bool PrivetRegisterOperationImpl::PrivetErrorTransient(
    const std::string& error) {
  return (error == kPrivetErrorDeviceBusy) ||
         (error == kPrivetErrorPendingUserAction);
}

// static
GURL PrivetRegisterOperationImpl::GetURLForActionAndUser(
    PrivetHTTPClientImpl* privet_client,
    const std::string& action,
    const std::string& user) {
  return GURL(base::StringPrintf(kPrivetRegisterURLFormat,
                                 privet_client->host_port().host().c_str(),
                                 privet_client->host_port().port(),
                                 action.c_str(),
                                 user.c_str()));
}

PrivetRegisterOperationImpl::Cancelation::Cancelation(
    PrivetHTTPClientImpl* privet_client,
    const std::string& user) {
  GURL url = GetURLForActionAndUser(privet_client,
                                    kPrivetActionCancel,
                                    user);
  url_fetcher_ = privet_client->fetcher_factory().CreateURLFetcher(
      url, net::URLFetcher::POST, this);
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

const std::string& PrivetHTTPClientImpl::GetName() {
  return name_;
}

void PrivetHTTPClientImpl::CacheInfo(const base::DictionaryValue* cached_info) {
  cached_info_.reset(cached_info->DeepCopy());
  std::string token;
  if (cached_info_->GetString(kPrivetInfoKeyToken, &token)) {
    fetcher_factory_.set_token(token);
  }
}

}  // namespace local_discovery
