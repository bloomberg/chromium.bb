// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_POLICY_IMPL_H_
#define CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_POLICY_IMPL_H_

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/common/origin_trials/trial_policy.h"

namespace content {

class OriginTrialPolicy;

// TrialPolicyImpl is an adaptor to fit the policy provided by the content
// embedder via ContentClient to the interface allowed by the DEPS rules of
// third_party/WebKit/common
// TODO(avallee, kinuko): Plumb the the content embedder policy straight through
// to the users in third_party/WebKit/common/origin_trials.
class CONTENT_EXPORT TrialPolicyImpl : public blink::TrialPolicy {
 public:
  bool IsOriginTrialsSupported() const override;

  base::StringPiece GetPublicKey() const override;
  bool IsFeatureDisabled(base::StringPiece feature) const override;
  bool IsTokenDisabled(base::StringPiece token_signature) const override;
  bool IsOriginSecure(const GURL& url) const override;

 private:
  const OriginTrialPolicy* policy() const;
};

}  // namespace content

#endif  // CONTENT_COMMON_ORIGIN_TRIALS_TRIAL_POLICY_IMPL_H_
