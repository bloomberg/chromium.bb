// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_trial_policy.h"

namespace content {

base::StringPiece OriginTrialPolicy::GetPublicKey() const {
  return base::StringPiece();
}

bool OriginTrialPolicy::IsFeatureDisabled(base::StringPiece feature) const {
  return false;
}

bool OriginTrialPolicy::IsTokenDisabled(
    base::StringPiece token_signature) const {
  return false;
}

}  // namespace content
