// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_
#define CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace content {

class ShellOriginTrialPolicy : public blink::OriginTrialPolicy {
 public:
  ShellOriginTrialPolicy();
  ~ShellOriginTrialPolicy() override;

  // blink::OriginTrialPolicy interface
  bool IsOriginTrialsSupported() const override;
  std::vector<base::StringPiece> GetPublicKeys() const override;
  bool IsOriginSecure(const GURL& url) const override;

 private:
  std::vector<base::StringPiece> public_keys_;

  DISALLOW_COPY_AND_ASSIGN(ShellOriginTrialPolicy);
};

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_SHELL_ORIGIN_TRIAL_POLICY_H_
