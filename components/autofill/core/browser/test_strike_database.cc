// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_strike_database.h"

#include "base/task/post_task.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/leveldb_proto/testing/test_proto_database_impl.h"

namespace autofill {

namespace {
const char kDatabaseClientName[] = "TestStrikeService";
}  // namespace

TestStrikeDatabase::TestStrikeDatabase(const base::FilePath& database_dir)
    : StrikeDatabase(database_dir) {
  db_ = std::make_unique<leveldb_proto::TestProtoDatabaseImpl<StrikeData>>(
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
  db_->Init(kDatabaseClientName, database_dir,
            leveldb_proto::CreateSimpleOptions(),
            base::BindRepeating(&TestStrikeDatabase::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

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

int TestStrikeDatabase::GetNumberOfDatabaseCalls() {
  leveldb_proto::TestProtoDatabaseImpl<StrikeData>* test_db =
      static_cast<leveldb_proto::TestProtoDatabaseImpl<StrikeData>*>(db_.get());
  return test_db->number_of_db_calls();
}

}  // namespace autofill
