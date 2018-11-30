// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_

#include <string>

#include "components/autofill/core/browser/strike_database.h"

namespace autofill {

// Implementation of StrikeDatabase for credit card saves (both local and
// upload).
class CreditCardSaveStrikeDatabase : public StrikeDatabase {
 public:
  CreditCardSaveStrikeDatabase(const base::FilePath& database_dir);
  ~CreditCardSaveStrikeDatabase() override;

  std::string GetProjectPrefix() override;
  int GetMaxStrikesLimit() override;
  long long GetExpiryTimeMicros() override;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CREDIT_CARD_SAVE_STRIKE_DATABASE_H_
