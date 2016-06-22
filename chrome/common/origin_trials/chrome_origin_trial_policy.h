// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ORIGIN_TRIALS_CHROME_ORIGIN_TRIAL_POLICY_H_
#define CHROME_COMMON_ORIGIN_TRIALS_CHROME_ORIGIN_TRIAL_POLICY_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "content/public/common/origin_trial_policy.h"

// This class is instantiated on the main/ui thread, but its methods can be
// accessed from any thread.
class ChromeOriginTrialPolicy : public content::OriginTrialPolicy {
 public:
  ChromeOriginTrialPolicy();
  ~ChromeOriginTrialPolicy() override;

  // OriginTrialPolicy interface
  base::StringPiece GetPublicKey() const override;

  bool SetPublicKeyFromASCIIString(const std::string& ascii_public_key);

 private:
  std::string public_key_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOriginTrialPolicy);
};

#endif  // CHROME_COMMON_ORIGIN_TRIALS_CHROME_ORIGIN_TRIAL_POLICY_H_
