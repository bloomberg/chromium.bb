// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_policy_impl.h"

#include "base/feature_list.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_trial_policy.h"
#include "content/public/common/origin_util.h"

namespace content {

bool TrialPolicyImpl::IsOriginTrialsSupported() const {
  // In order for the validator to work these are all required.
  return base::FeatureList::IsEnabled(features::kOriginTrials) && policy() &&
         !GetPublicKey().empty();
}
base::StringPiece TrialPolicyImpl::GetPublicKey() const {
  return policy()->GetPublicKey();
}
bool TrialPolicyImpl::IsFeatureDisabled(base::StringPiece feature) const {
  return policy()->IsFeatureDisabled(feature);
}
bool TrialPolicyImpl::IsTokenDisabled(base::StringPiece token_signature) const {
  return policy()->IsTokenDisabled(token_signature);
}
bool TrialPolicyImpl::IsOriginSecure(const GURL& url) const {
  return ::content::IsOriginSecure(url);
}

const OriginTrialPolicy* TrialPolicyImpl::policy() const {
  return GetContentClient()->GetOriginTrialPolicy();
}

}  // namespace content
