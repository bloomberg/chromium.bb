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
// They will be LevelDB.*.BudgetManager. Changes here should be synchronized
// with histograms.xml.
const char kDatabaseUMAName[] = "BudgetManager";

// The default amount of time during which a budget will be valid.
// This is 3 days = 72 hours.
constexpr double kBudgetDurationInHours = 72;

}  // namespace

BudgetDatabase::BudgetInfo::BudgetInfo() {}

BudgetDatabase::BudgetInfo::BudgetInfo(const BudgetInfo&& other)
    : last_engagement_award(other.last_engagement_award) {
  chunks = std::move(other.chunks);
}

BudgetDatabase::BudgetInfo::~BudgetInfo() {}

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
  if (IsCached(origin)) {
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

  // Add a new chunk of budget for the origin at the default expiration time.
  base::Time expiration =
      clock_->Now() + base::TimeDelta::FromHours(kBudgetDurationInHours);
  budget_map_[origin.spec()].chunks.emplace_back(amount, expiration);

  // Now that the cache is updated, write the data to the database.
  WriteCachedValuesToDatabase(origin, callback);
}

void BudgetDatabase::AddEngagementBudget(const GURL& origin,
                                         double score,
                                         const StoreBudgetCallback& callback) {
  DCHECK_EQ(origin.GetOrigin(), origin);

  // By default we award the "full" award. Then that ratio is decreased if
  // there have been other awards recently.
  double ratio = 1.0;

  // Calculate how much budget should be awarded. If the origin is not cached,
  // then we award a full amount.
  if (IsCached(origin)) {
    base::TimeDelta elapsed =
        clock_->Now() - budget_map_[origin.spec()].last_engagement_award;
    int elapsed_hours = elapsed.InHours();
    if (elapsed_hours == 0) {
      // Don't give engagement awards for periods less than an hour.
      callback.Run(true);
      return;
    }
    if (elapsed_hours < kBudgetDurationInHours)
      ratio = elapsed_hours / kBudgetDurationInHours;
  }

  // Update the last_engagement_award to the current time. If the origin wasn't
  // already in the map, this adds a new entry for it.
  budget_map_[origin.spec()].last_engagement_award = clock_->Now();

  // Pass to the base AddBudget to update the cache and write to the database.
  AddBudget(origin, score * ratio, callback);
}

void BudgetDatabase::SpendBudget(const GURL& origin,
                                 double amount,
                                 const StoreBudgetCallback& callback) {
  DCHECK_EQ(origin.GetOrigin(), origin);

  // First, cleanup any expired budget chunks for the origin.
  CleanupExpiredBudget(origin);

  if (!IsCached(origin)) {
    callback.Run(false);
    return;
  }

  // Walk the list of budget chunks to see if the origin has enough budget.
  double total = 0;
  BudgetInfo& info = budget_map_[origin.spec()];
  for (const BudgetChunk& chunk : info.chunks)
    total += chunk.amount;

  if (total < amount) {
    callback.Run(false);
    return;
  }

  // Walk the chunks and remove enough budget to cover the needed amount.
  double bill = amount;
  for (auto iter = info.chunks.begin(); iter != info.chunks.end();) {
    if (iter->amount > bill) {
      iter->amount -= bill;
      bill = 0;
      ++iter;
      break;
    }
    bill -= iter->amount;
    iter = info.chunks.erase(iter);
  }

  // There should have been enough budget to cover the entire bill.
  DCHECK_EQ(0, bill);

  // Now that the cache is updated, write the data to the database.
  // TODO(harkness): Consider adding a second parameter to the callback so the
  // caller can distinguish between not enough budget and a failed database
  // write.
  // TODO(harkness): If the database write fails, the cache will be out of sync
  // with the database. Consider ways to mitigate this.
  WriteCachedValuesToDatabase(origin, callback);
}

void BudgetDatabase::OnDatabaseInit(bool success) {
  // TODO(harkness): Consider caching the budget database now?
}

bool BudgetDatabase::IsCached(const GURL& origin) const {
  return budget_map_.find(origin.spec()) != budget_map_.end();
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
  BudgetInfo& info = budget_map_[origin.spec()];
  for (const auto& chunk : budget_proto->budget()) {
    info.chunks.emplace_back(chunk.amount(),
                             base::Time::FromInternalValue(chunk.expiration()));
  }

  info.last_engagement_award =
      base::Time::FromInternalValue(budget_proto->engagement_last_updated());

  callback.Run(success);
}

void BudgetDatabase::DidGetBudget(const GURL& origin,
                                  const GetBudgetDetailsCallback& callback,
                                  bool success) {
  // If the database wasn't able to read the information, return the
  // failure and an empty BudgetPrediction.
  if (!success) {
    callback.Run(success, BudgetPrediction());
    return;
  }

  // First, cleanup any expired budget chunks for the origin.
  CleanupExpiredBudget(origin);

  // Now, build up the BudgetExpection. This is different from the format
  // in which the cache stores the data. The cache stores chunks of budget and
  // when that budget expires. The BudgetPrediction describes a set of times
  // and the budget at those times.
  BudgetPrediction prediction;
  double total = 0;

  if (IsCached(origin)) {
    // Starting with the chunks that expire the farthest in the future, build up
    // the budget predictions for those future times.
    const BudgetChunks& chunks = budget_map_[origin.spec()].chunks;
    for (const auto& chunk : base::Reversed(chunks)) {
      prediction.emplace_front(total, chunk.expiration);
      total += chunk.amount;
    }
  }

  // Always add one entry at the front of the list for the total budget now.
  prediction.emplace_front(total, clock_->Now());

  callback.Run(true /* success */, prediction);
}

void BudgetDatabase::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

void BudgetDatabase::WriteCachedValuesToDatabase(
    const GURL& origin,
    const StoreBudgetCallback& callback) {
  // First, cleanup any expired budget chunks for the origin.
  CleanupExpiredBudget(origin);

  // Create the data structures that are passed to the ProtoDatabase.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<budget_service::Budget>::KeyEntryVector>
      entries(new leveldb_proto::ProtoDatabase<
              budget_service::Budget>::KeyEntryVector());
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  // Each operation can either update the existing budget or remove the origin's
  // budget information.
  if (IsCached(origin)) {
    // Build the Budget proto object.
    budget_service::Budget budget;
    const BudgetInfo& info = budget_map_[origin.spec()];
    for (const auto& chunk : info.chunks) {
      budget_service::BudgetChunk* budget_chunk = budget.add_budget();
      budget_chunk->set_amount(chunk.amount);
      budget_chunk->set_expiration(chunk.expiration.ToInternalValue());
    }
    budget.set_engagement_last_updated(
        info.last_engagement_award.ToInternalValue());
    entries->push_back(std::make_pair(origin.spec(), budget));
  } else {
    // If the origin doesn't exist in the cache, this is a remove operation.
    keys_to_remove->push_back(origin.spec());
  }

  // Send the updates to the database.
  db_->UpdateEntries(std::move(entries), std::move(keys_to_remove), callback);
}

void BudgetDatabase::CleanupExpiredBudget(const GURL& origin) {
  if (!IsCached(origin))
    return;

  base::Time now = clock_->Now();

  BudgetChunks& chunks = budget_map_[origin.spec()].chunks;
  auto cleanup_iter = chunks.begin();

  // This relies on the list of chunks being in timestamp order.
  while (cleanup_iter != chunks.end() && cleanup_iter->expiration <= now)
    cleanup_iter = chunks.erase(cleanup_iter);

  // If the entire budget is empty now AND there have been no engagements
  // in the last kBudgetDurationInHours hours, remove this from the cache.
  if (chunks.empty() &&
      budget_map_[origin.spec()].last_engagement_award <
          clock_->Now() - base::TimeDelta::FromHours(kBudgetDurationInHours))
    budget_map_.erase(origin.spec());
}
