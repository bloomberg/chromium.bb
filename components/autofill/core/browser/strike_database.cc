// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace autofill {

namespace {
const char kDatabaseClientName[] = "StrikeService";
const char kKeyDeliminator[] = "__";
const int kMaxInitAttempts = 3;
}  // namespace

StrikeDatabase::StrikeDatabase(const base::FilePath& database_dir)
    : db_(std::make_unique<leveldb_proto::ProtoDatabaseImpl<StrikeData>>(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}))),
      database_dir_(database_dir),
      weak_ptr_factory_(this) {
  db_->Init(kDatabaseClientName, database_dir,
            leveldb_proto::CreateSimpleOptions(),
            base::BindRepeating(&StrikeDatabase::OnDatabaseInit,
                                weak_ptr_factory_.GetWeakPtr()));
}

StrikeDatabase::~StrikeDatabase() {}

bool StrikeDatabase::IsMaxStrikesLimitReached(const std::string id) {
  return GetStrikes(id) >= GetMaxStrikesLimit();
}

int StrikeDatabase::AddStrike(const std::string id) {
  std::string key = GetKey(id);
  int num_strikes = strike_map_cache_.count(key)  // Cache has entry for |key|.
                        ? strike_map_cache_[key].num_strikes() + 1
                        : 1;
  SetStrikeData(key, num_strikes);
  base::UmaHistogramCounts1000(
      "Autofill.StrikeDatabase.NthStrikeAdded." + GetProjectPrefix(),
      num_strikes);
  return num_strikes;
}

int StrikeDatabase::RemoveStrike(const std::string id) {
  std::string key = GetKey(id);
  DCHECK(strike_map_cache_.count(key));
  int num_strikes = strike_map_cache_[key].num_strikes() - 1;
  if (num_strikes < 1) {
    ClearStrikes(id);
    return 0;
  }
  SetStrikeData(key, num_strikes);
  return num_strikes;
}

int StrikeDatabase::GetStrikes(const std::string id) {
  std::string key = GetKey(id);
  return strike_map_cache_.count(key)  // Cache contains entry for |key|.
             ? strike_map_cache_[key].num_strikes()
             : 0;
}

void StrikeDatabase::ClearStrikes(const std::string id) {
  std::string key = GetKey(id);
  strike_map_cache_.erase(key);
  ClearAllProtoStrikesForKey(key, base::DoNothing());
}

StrikeDatabase::StrikeDatabase()
    : db_(nullptr),
      database_dir_(base::FilePath(nullptr)),
      weak_ptr_factory_(this) {}

void StrikeDatabase::OnDatabaseInit(bool success) {
  database_initialized_ = success;
  if (!success) {
    base::UmaHistogramCounts100(
        "Autofill.StrikeDatabase.StrikeDatabaseInitFailed", num_init_attempts_);
    if (num_init_attempts_ < kMaxInitAttempts) {
      num_init_attempts_++;
      db_->Init(kDatabaseClientName, database_dir_,
                leveldb_proto::CreateSimpleOptions(),
                base::BindRepeating(&StrikeDatabase::OnDatabaseInit,
                                    weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }
  db_->LoadKeysAndEntries(
      base::BindRepeating(&StrikeDatabase::OnDatabaseLoadKeysAndEntries,
                          weak_ptr_factory_.GetWeakPtr()));
}

void StrikeDatabase::OnDatabaseLoadKeysAndEntries(
    bool success,
    std::unique_ptr<std::map<std::string, StrikeData>> entries) {
  if (!success) {
    database_initialized_ = false;
    return;
  }
  strike_map_cache_.insert(entries->begin(), entries->end());
  // Remove all expired strikes.
  for (auto entry : *entries) {
    if (AutofillClock::Now().ToDeltaSinceWindowsEpoch().InMicroseconds() -
            entry.second.last_update_timestamp() >
        GetExpiryTimeMicros()) {
      if (GetStrikes(GetIdPartFromKey(entry.first)) > 0)
        RemoveStrike(GetIdPartFromKey(entry.first));
    }
  }
}

std::string StrikeDatabase::GetKey(const std::string id) {
  return GetProjectPrefix() + kKeyDeliminator + id;
}

std::string StrikeDatabase::GetIdPartFromKey(const std::string key) {
  return key.substr((GetProjectPrefix() + kKeyDeliminator).size());
}

void StrikeDatabase::SetStrikeData(const std::string key, int num_strikes) {
  StrikeData data;
  data.set_num_strikes(num_strikes);
  data.set_last_update_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  UpdateCache(key, data);
  SetProtoStrikeData(key, data, base::DoNothing());
}

void StrikeDatabase::GetProtoStrikes(const std::string key,
                                     const StrikesCallback& outer_callback) {
  if (!database_initialized_) {
    outer_callback.Run(false);
    return;
  }
  GetProtoStrikeData(
      key,
      base::BindRepeating(&StrikeDatabase::OnGetProtoStrikes,
                          base::Unretained(this), std::move(outer_callback)));
}

void StrikeDatabase::ClearAllProtoStrikes(
    const ClearStrikesCallback& outer_callback) {
  if (!database_initialized_) {
    outer_callback.Run(false);
    return;
  }
  // For deleting all, filter method always returns true.
  db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<StrikeDataProto::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      outer_callback);
}

void StrikeDatabase::ClearAllProtoStrikesForKey(
    const std::string& key,
    const ClearStrikesCallback& outer_callback) {
  if (!database_initialized_) {
    outer_callback.Run(false);
    return;
  }
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  keys_to_remove->push_back(key);
  db_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector>(),
      /*keys_to_remove=*/std::move(keys_to_remove), outer_callback);
}

void StrikeDatabase::GetProtoStrikeData(const std::string key,
                                        const GetValueCallback& callback) {
  if (!database_initialized_) {
    callback.Run(false, nullptr);
    return;
  }
  db_->GetEntry(key, callback);
}

void StrikeDatabase::SetProtoStrikeData(const std::string& key,
                                        const StrikeData& data,
                                        const SetValueCallback& callback) {
  if (!database_initialized_) {
    callback.Run(false);
    return;
  }
  std::unique_ptr<StrikeDataProto::KeyEntryVector> entries(
      new StrikeDataProto::KeyEntryVector());
  entries->push_back(std::make_pair(key, data));
  db_->UpdateEntries(
      /*entries_to_save=*/std::move(entries),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      callback);
}

void StrikeDatabase::OnGetProtoStrikes(
    StrikesCallback callback,
    bool success,
    std::unique_ptr<StrikeData> strike_data) {
  if (success && strike_data)
    callback.Run(strike_data->num_strikes());
  else
    callback.Run(0);
}

void StrikeDatabase::LoadKeys(const LoadKeysCallback& callback) {
  db_->LoadKeys(callback);
}

void StrikeDatabase::UpdateCache(const std::string& key,
                                 const StrikeData& data) {
  strike_map_cache_[key] = data;
}

}  // namespace autofill
