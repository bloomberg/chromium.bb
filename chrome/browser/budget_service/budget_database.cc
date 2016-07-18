// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include "chrome/browser/budget_service/budget.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

// UMA are logged for the database with this string as part of the name.
// They will be LevelDB.*.BackgroundBudgetService. Changes here should be
// synchronized with histograms.xml.
const char kDatabaseUMAName[] = "BackgroundBudgetService";

}  // namespace

BudgetDatabase::BudgetDatabase(
    const base::FilePath& database_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : db_(new leveldb_proto::ProtoDatabaseImpl<budget_service::Budget>(
          task_runner)),
      weak_ptr_factory_(this) {
  db_->Init(kDatabaseUMAName, database_dir,
            base::Bind(&BudgetDatabase::OnDatabaseInit,
                       weak_ptr_factory_.GetWeakPtr()));
}

BudgetDatabase::~BudgetDatabase() {}

void BudgetDatabase::OnDatabaseInit(bool success) {
  // TODO(harkness): Consider caching the budget database now?
}

void BudgetDatabase::GetValue(const GURL& origin,
                              const GetValueCallback& callback) {
  DCHECK_EQ(origin.GetOrigin(), origin);
  db_->GetEntry(origin.spec(), callback);
}

void BudgetDatabase::SetValue(const GURL& origin,
                              const budget_service::Budget& budget,
                              const SetValueCallback& callback) {
  DCHECK_EQ(origin.GetOrigin(), origin);

  // Build structures to hold the updated values.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<budget_service::Budget>::KeyEntryVector>
      entries(new leveldb_proto::ProtoDatabase<
              budget_service::Budget>::KeyEntryVector());
  entries->push_back(std::make_pair(origin.spec(), budget));
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  // Send the updates to the database.
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove), callback);
}
