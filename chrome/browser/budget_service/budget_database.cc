// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include "base/containers/adapters.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/budget_service/budget.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// UMA are logged for the database with this string as part of the name.
// They will be LevelDB.*.BackgroundBudgetService. Changes here should be
// synchronized with histograms.xml.
const char kDatabaseUMAName[] = "BackgroundBudgetService";

// The default amount of time during which a budget will be valid.
// This is 3 days = 72 hours.
constexpr double kBudgetDurationInHours = 72;

}  // namespace

BudgetDatabase::BudgetDatabase(
    const base::FilePath& database_dir,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : db_(new leveldb_proto::ProtoDatabaseImpl<budget_service::Budget>(
          task_runner)),
      clock_(base::WrapUnique(new base::DefaultClock)),
      weak_ptr_factory_(this) {
  db_->Init(kDatabaseUMAName, database_dir,
            base::Bind(&BudgetDatabase::OnDatabaseInit,
                       weak_ptr_factory_.GetWeakPtr()));
}

BudgetDatabase::~BudgetDatabase() {}

void BudgetDatabase::GetBudgetDetails(
    const GURL& origin,
    const GetBudgetDetailsCallback& callback) {
  DCHECK_EQ(origin.GetOrigin(), origin);

  // If this origin is already in the cache, immediately return the data.
  if (budget_map_.find(origin.spec()) != budget_map_.end()) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&BudgetDatabase::DidGetBudget,
                                       weak_ptr_factory_.GetWeakPtr(), origin,
                                       callback, true /* success */));
    return;
  }

  // Otherwise, query for the data, add it to the cache, then return the result.
  AddToCacheCallback cache_callback =
      base::Bind(&BudgetDatabase::DidGetBudget, weak_ptr_factory_.GetWeakPtr(),
                 origin, callback);
  db_->GetEntry(origin.spec(), base::Bind(&BudgetDatabase::AddToCache,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          origin, cache_callback));
}

void BudgetDatabase::AddBudget(const GURL& origin,
                               double amount,
                               const StoreBudgetCallback& callback) {
  DCHECK_EQ(origin.GetOrigin(), origin);

  // Look up the origin in our cache. Adding budget without first querying the
  // existing budget is not suported.
  DCHECK_GT(budget_map_.count(origin.spec()), 0U);

  base::Time expiration =
      clock_->Now() + base::TimeDelta::FromHours(kBudgetDurationInHours);
  budget_map_[origin.spec()].second.push_back(
      std::make_pair(amount, expiration.ToInternalValue()));

  // Now that the cache is updated, write the data to the database.
  WriteCachedValuesToDatabase(origin, callback);
}

void BudgetDatabase::WriteCachedValuesToDatabase(
    const GURL& origin,
    const StoreBudgetCallback& callback) {
  // Create the data structures that are passed to the ProtoDatabase.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<budget_service::Budget>::KeyEntryVector>
      entries(new leveldb_proto::ProtoDatabase<
              budget_service::Budget>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  // Each operation can either update the existing budget or remove the origin's
  // budget information.
  if (budget_map_.find(origin.spec()) == budget_map_.end()) {
    // If the origin doesn't exist in the cache, this is a remove operation.
    keys_to_remove->push_back(origin.spec());
  } else {
    // Build the Budget proto object.
    budget_service::Budget budget;
    const BudgetInfo& info = budget_map_[origin.spec()];
    budget.set_debt(info.first);
    for (const auto& chunk : info.second) {
      budget_service::BudgetChunk* budget_chunk = budget.add_budget();
      budget_chunk->set_amount(chunk.first);
      budget_chunk->set_expiration(chunk.second);
    }
    entries->push_back(std::make_pair(origin.spec(), budget));
  }

  // Send the updates to the database.
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove), callback);
}

void BudgetDatabase::OnDatabaseInit(bool success) {
  // TODO(harkness): Consider caching the budget database now?
}

void BudgetDatabase::AddToCache(
    const GURL& origin,
    const AddToCacheCallback& callback,
    bool success,
    std::unique_ptr<budget_service::Budget> budget_proto) {
  // If the database read failed, there's nothing to add to the cache.
  if (!success || !budget_proto) {
    callback.Run(success);
    return;
  }

  // Add the data to the cache, converting from the proto format to an STL
  // format which is better for removing things from the list.
  BudgetChunks chunks;
  for (const auto& chunk : budget_proto->budget())
    chunks.push_back(std::make_pair(chunk.amount(), chunk.expiration()));

  DCHECK(budget_proto->has_debt());
  budget_map_[origin.spec()] =
      std::make_pair(budget_proto->debt(), std::move(chunks));

  callback.Run(success);
}

void BudgetDatabase::DidGetBudget(const GURL& origin,
                                  const GetBudgetDetailsCallback& callback,
                                  bool success) {
  // If the database wasn't able to read the information, return the
  // failure and an empty BudgetExpectation.
  if (!success) {
    callback.Run(success, 0, BudgetExpectation());
    return;
  }

  // Otherwise, build up the BudgetExpection. This is different from the format
  // in which the cache stores the data. The cache stores chunks of budget and
  // when that budget expires. The BudgetExpectation describes a set of times
  // and the budget at those times.
  const BudgetInfo& info = budget_map_[origin.spec()];
  BudgetExpectation expectation;
  double total = 0;

  // Starting with the chunks that expire the farthest in the future, build up
  // the budget expectations for those future times.
  for (const auto& chunk : base::Reversed(info.second)) {
    expectation.push_front(std::make_pair(total, chunk.second));
    total += chunk.first;
  }

  // Always add one entry at the front of the list for the total budget right
  // now.
  expectation.push_front(std::make_pair(total, 0));

  callback.Run(true /* success */, info.first, expectation);
}

void BudgetDatabase::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}
