// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/http_auth_handler_data_reduction_proxy.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/i18n/icu_string_conversions.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_request_info.h"

namespace data_reduction_proxy {

using net::AuthCredentials;
using net::BoundNetLog;
using net::CompletionCallback;
using net::HttpAuth;
using net::HttpAuthChallengeTokenizer;
using net::HttpAuthHandler;
using net::HttpAuthHandlerFactory;
using net::HttpRequestInfo;
using net::HttpUtil;

HttpAuthHandlerDataReductionProxy::Factory::Factory(
    const std::vector<GURL>& authorized_spdyproxy_origins) {
  for (unsigned int i = 0; i < authorized_spdyproxy_origins.size(); ++i) {
    if (authorized_spdyproxy_origins[i].possibly_invalid_spec().empty()) {
      VLOG(1) << "SpdyProxy auth without configuring authorized origin.";
      return;
    }
  }
  authorized_spdyproxy_origins_ = authorized_spdyproxy_origins;
}

HttpAuthHandlerDataReductionProxy::Factory::~Factory() {
}

int HttpAuthHandlerDataReductionProxy::Factory::CreateAuthHandler(
    HttpAuthChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    CreateReason reason,
    int digest_nonce_count,
    const BoundNetLog& net_log,
    scoped_ptr<HttpAuthHandler>* handler) {
  // If a spdyproxy auth proxy has not been set, refuse all requests to use this
  // auth handler.
  if (authorized_spdyproxy_origins_.empty())
    return net::ERR_UNSUPPORTED_AUTH_SCHEME;

  // We ensure that this authentication handler is used only with an authorized
  // SPDY proxy, since otherwise a user's authentication token can be
  // sniffed by a malicious proxy that presents an appropriate challenge.
  const GURL origin_origin = origin.GetOrigin();
  if (!(std::find(authorized_spdyproxy_origins_.begin(),
                  authorized_spdyproxy_origins_.end(),
                  origin_origin) != authorized_spdyproxy_origins_.end())) {
    UMA_HISTOGRAM_COUNTS("Net.UnexpectedSpdyProxyAuth", 1);
    VLOG(1) << "SpdyProxy auth request with an unexpected config."
            << " origin: " << origin_origin.possibly_invalid_spec();
    return net::ERR_UNSUPPORTED_AUTH_SCHEME;
  }

  scoped_ptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerDataReductionProxy());
  if (!tmp_handler->InitFromChallenge(challenge, target, origin, net_log))
    return net::ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return net::OK;
}

HttpAuth::AuthorizationResult
HttpAuthHandlerDataReductionProxy::HandleAnotherChallenge(
    HttpAuthChallengeTokenizer* challenge) {
  // SpdyProxy authentication is always a single round, so any responses
  // should be treated as a rejection.
  return HttpAuth::AUTHORIZATION_RESULT_REJECT;
}

bool HttpAuthHandlerDataReductionProxy::NeedsIdentity() {
  return true;
}

bool HttpAuthHandlerDataReductionProxy::AllowsDefaultCredentials() {
  return false;
}

bool HttpAuthHandlerDataReductionProxy::AllowsExplicitCredentials() {
  return true;
}

HttpAuthHandlerDataReductionProxy::~HttpAuthHandlerDataReductionProxy() {}

bool HttpAuthHandlerDataReductionProxy::Init(
    HttpAuthChallengeTokenizer* challenge) {
  auth_scheme_ = HttpAuth::AUTH_SCHEME_SPDYPROXY;
  score_ = 5;
  properties_ = ENCRYPTS_IDENTITY;
  return ParseChallenge(challenge);
}

int HttpAuthHandlerDataReductionProxy::GenerateAuthTokenImpl(
    const AuthCredentials* credentials, const HttpRequestInfo* request,
    const CompletionCallback&, std::string* auth_token) {
  DCHECK(credentials);
  if (credentials->password().length() == 0) {
    DVLOG(1) << "Received a SpdyProxy auth token request without an "
             << "available token.";
    return -1;
  }
  *auth_token = "SpdyProxy ps=\"" + ps_token_ + "\", sid=\"" +
      base::UTF16ToUTF8(credentials->password()) + "\"";
  return net::OK;
}

bool HttpAuthHandlerDataReductionProxy::ParseChallenge(
    HttpAuthChallengeTokenizer* challenge) {

  // Verify the challenge's auth-scheme.
  if (!LowerCaseEqualsASCII(challenge->scheme(), "spdyproxy")) {
    VLOG(1) << "Parsed challenge without SpdyProxy type";
    return false;
  }

  HttpUtil::NameValuePairsIterator parameters = challenge->param_pairs();

  // Loop through all the properties.
  while (parameters.GetNext()) {
    // FAIL -- couldn't parse a property.
    if (!ParseChallengeProperty(parameters.name(),
                                parameters.value()))
      return false;
  }
  // Check if tokenizer failed.
  if (!parameters.valid())
    return false;

  // Check that the required properties were provided.
  if (realm_.empty())
    return false;

  if (ps_token_.empty())
    return false;

  return true;
}

bool HttpAuthHandlerDataReductionProxy::ParseChallengeProperty(
    const std::string& name, const std::string& value) {
  if (LowerCaseEqualsASCII(name, "realm")) {
    std::string realm;
    if (!base::ConvertToUtf8AndNormalize(value, base::kCodepageLatin1, &realm))
      return false;
    realm_ = realm;
  } else if (LowerCaseEqualsASCII(name, "ps")) {
    ps_token_ = value;
  } else {
    VLOG(1) << "Skipping unrecognized SpdyProxy auth property, " << name;
  }
  return true;
}

}  // namespace data_reduction_proxy
