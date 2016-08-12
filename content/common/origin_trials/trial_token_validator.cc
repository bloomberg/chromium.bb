// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token_validator.h"

#include "base/feature_list.h"
#include "base/time/time.h"
#include "content/common/origin_trials/trial_token.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_trial_policy.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "third_party/WebKit/public/platform/WebOriginTrialTokenStatus.h"

namespace content {

blink::WebOriginTrialTokenStatus TrialTokenValidator::ValidateToken(
    const std::string& token,
    const url::Origin& origin,
    std::string* feature_name) {
  ContentClient* content_client = GetContentClient();
  const OriginTrialPolicy* origin_trial_policy =
      content_client->GetOriginTrialPolicy();
  if (!origin_trial_policy)
    return blink::WebOriginTrialTokenStatus::NotSupported;

  // TODO(iclelland): Allow for multiple signing keys, and iterate over all
  // active keys here. https://crbug.com/543220
  base::StringPiece public_key = origin_trial_policy->GetPublicKey();
  if (public_key.empty())
    return blink::WebOriginTrialTokenStatus::NotSupported;

  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> trial_token =
      TrialToken::From(token, public_key, &status);
  if (status != blink::WebOriginTrialTokenStatus::Success)
    return status;

  status = trial_token->IsValid(origin, base::Time::Now());
  if (status != blink::WebOriginTrialTokenStatus::Success)
    return status;

  if (origin_trial_policy->IsFeatureDisabled(trial_token->feature_name()))
    return blink::WebOriginTrialTokenStatus::FeatureDisabled;

  *feature_name = trial_token->feature_name();
  return blink::WebOriginTrialTokenStatus::Success;
}

bool TrialTokenValidator::RequestEnablesFeature(
    const net::URLRequest* request,
    base::StringPiece feature_name) {
  // TODO(mek): Possibly cache the features that are availble for request in
  // UserData associated with the request.
  return RequestEnablesFeature(request->url(), request->response_headers(),
                               feature_name);
}

bool TrialTokenValidator::RequestEnablesFeature(
    const GURL& request_url,
    const net::HttpResponseHeaders* response_headers,
    base::StringPiece feature_name) {
  if (!base::FeatureList::IsEnabled(features::kOriginTrials))
    return false;

  if (!IsOriginSecure(request_url))
    return false;

  url::Origin origin(request_url);
  size_t iter = 0;
  std::string token;
  while (response_headers->EnumerateHeader(&iter, "Origin-Trial", &token)) {
    std::string token_feature;
    // TODO(mek): Log the validation errors to histograms?
    if (ValidateToken(token, origin, &token_feature) ==
        blink::WebOriginTrialTokenStatus::Success)
      if (token_feature == feature_name)
        return true;
  }
  return false;
}

}  // namespace content
