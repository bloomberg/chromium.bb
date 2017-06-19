// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_

#include <string>

#include "components/signin/core/browser/signin_header_helper.h"

class GURL;

namespace signin {

// SigninHeaderHelper implementation managing the Dice header.
class DiceHeaderHelper : public SigninHeaderHelper {
 public:
  DiceHeaderHelper() {}
  ~DiceHeaderHelper() override {}

  // Returns the parameters contained in the X-Chrome-ID-Consistency-Response
  // response header.
  static DiceResponseParams BuildDiceResponseParams(
      const std::string& header_value);

  // Returns the header value for Dice requests. Returns the empty string when
  // the header must not be added.
  std::string BuildRequestHeader(const std::string& account_id,
                                 bool sync_enabled);

 private:
  // SigninHeaderHelper implementation:
  bool IsUrlEligibleForRequestHeader(const GURL& url) override;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_DICE_HEADER_HELPER_H_
