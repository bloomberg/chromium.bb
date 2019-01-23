// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_

#include "components/autofill/core/browser/local_card_migration_strike_database.h"

namespace autofill {

class TestLocalCardMigrationStrikeDatabase
    : public LocalCardMigrationStrikeDatabase {
 public:
  TestLocalCardMigrationStrikeDatabase(StrikeDatabase* strike_database);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_LOCAL_CARD_MIGRATION_STRIKE_DATABASE_H_
