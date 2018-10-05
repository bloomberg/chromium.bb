// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_STRIKE_DATABASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_STRIKE_DATABASE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/autofill/core/browser/strike_database.h"

namespace autofill {

class TestStrikeDatabase : public StrikeDatabase {
 public:
  explicit TestStrikeDatabase(const base::FilePath& database_dir);

  void AddEntries(
      std::vector<std::pair<std::string, StrikeData>> entries_to_add,
      const SetValueCallback& callback);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_STRIKE_DATABASE_H_
