// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_local_card_migration_strike_database.h"

namespace autofill {

TestLocalCardMigrationStrikeDatabase::TestLocalCardMigrationStrikeDatabase(
    StrikeDatabase* strike_database)
    : LocalCardMigrationStrikeDatabase(strike_database) {}

}  // namespace autofill
