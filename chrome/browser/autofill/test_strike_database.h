// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_TEST_STRIKE_DATABASE_H_
#define CHROME_BROWSER_AUTOFILL_TEST_STRIKE_DATABASE_H_

#include "chrome/browser/autofill/strike_database.h"

namespace autofill {

class TestStrikeDatabase : public StrikeDatabase {
 public:
  TestStrikeDatabase(const base::FilePath& database_dir);

  void AddEntries(
      std::vector<std::pair<std::string, StrikeData>> entries_to_add,
      const SetValueCallback& callback);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_TEST_STRIKE_DATABASE_H_
