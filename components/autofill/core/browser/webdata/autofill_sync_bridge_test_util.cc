// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"

namespace autofill {

AutofillProfile CreateServerProfile(const std::string& server_id) {
  // TODO(sebsg): Set data.
  return AutofillProfile(AutofillProfile::SERVER_PROFILE, server_id);
}

CreditCard CreateServerCreditCard(const std::string& server_id) {
  // TODO(sebsg): Set data.
  return CreditCard(CreditCard::MASKED_SERVER_CARD, server_id);
}

}  // namespace autofill