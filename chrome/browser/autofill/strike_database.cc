// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/strike_database.h"

#include "base/task/post_task.h"
#include "chrome/browser/autofill/strike_data.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace autofill {

namespace {

const char kDatabaseClientName[] = "StrikeService";

}  // namespace

StrikeDatabase::StrikeDatabase(const base::FilePath& database_dir)
    : db_(std::make_unique<leveldb_proto::ProtoDatabaseImpl<StrikeData>>(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))),
      weak_ptr_factory_(this) {
  db_->Init(kDatabaseClientName, database_dir,
            leveldb_proto::CreateSimpleOptions(),
            base::BindRepeating(&StrikeDatabase::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

StrikeDatabase::~StrikeDatabase() {}

void StrikeDatabase::OnDatabaseInit(bool success) {}

}  // namespace autofill
