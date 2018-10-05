// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

TestStrikeDatabase::TestStrikeDatabase(const base::FilePath& database_dir)
    : StrikeDatabase(database_dir) {}

void TestStrikeDatabase::AddEntries(
    std::vector<std::pair<std::string, StrikeData>> entries_to_add,
    const SetValueCallback& callback) {
  std::unique_ptr<leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector>
      entries(new leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector());
  for (std::pair<std::string, StrikeData> entry : entries_to_add) {
    entries->push_back(entry);
  }
  db_->UpdateEntries(
      /*entries_to_save=*/std::move(entries),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      callback);
}

}  // namespace autofill
