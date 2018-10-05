// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace autofill {

namespace {
const char kDatabaseClientName[] = "StrikeService";
const char kKeyDeliminator[] = "__";
const char kKeyPrefixForCreditCardSave[] = "creditCardSave";
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

void StrikeDatabase::GetStrikes(const std::string& key,
                                const StrikesCallback& outer_callback) {
  GetStrikeData(key, base::BindRepeating(&StrikeDatabase::OnGetStrikes,
                                         base::Unretained(this),
                                         std::move(outer_callback), key));
}

void StrikeDatabase::AddStrike(const std::string& key,
                               const StrikesCallback& outer_callback) {
  GetStrikeData(key, base::BindRepeating(&StrikeDatabase::OnAddStrike,
                                         base::Unretained(this),
                                         std::move(outer_callback), key));
}

void StrikeDatabase::ClearAllStrikesForKey(
    const std::string& key,
    const ClearStrikesCallback& outer_callback) {
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  keys_to_remove->push_back(key);
  db_->UpdateEntries(
      /*entries_to_save=*/std::make_unique<
          leveldb_proto::ProtoDatabase<StrikeData>::KeyEntryVector>(),
      /*keys_to_remove=*/std::move(keys_to_remove),
      base::BindRepeating(&StrikeDatabase::OnClearAllStrikesForKey,
                          base::Unretained(this), outer_callback, key));
}

void StrikeDatabase::ClearCache() {
  strike_map_cache_.clear();
}

std::string StrikeDatabase::GetKeyForCreditCardSave(
    const std::string& card_last_four_digits) {
  return CreateKey(GetKeyPrefixForCreditCardSave(), card_last_four_digits);
}

void StrikeDatabase::OnDatabaseInit(bool success) {}

void StrikeDatabase::GetStrikeData(const std::string& key,
                                   const GetValueCallback& inner_callback) {
  std::unordered_map<std::string, StrikeData>::iterator it =
      strike_map_cache_.find(key);
  if (it != strike_map_cache_.end()) {  // key is in cache
    StrikeData data_copy(it->second);
    inner_callback.Run(/*success=*/true,
                       std::make_unique<StrikeData>(data_copy));
  } else {
    db_->GetEntry(key, inner_callback);
  }
}

void StrikeDatabase::SetStrikeData(const std::string& key,
                                   const StrikeData& data,
                                   const SetValueCallback& inner_callback) {
  std::unique_ptr<StrikeDataProto::KeyEntryVector> entries(
      new StrikeDataProto::KeyEntryVector());
  entries->push_back(std::make_pair(key, data));
  db_->UpdateEntries(
      /*entries_to_save=*/std::move(entries),
      /*keys_to_remove=*/std::make_unique<std::vector<std::string>>(),
      inner_callback);
}

void StrikeDatabase::OnGetStrikes(StrikesCallback outer_callback,
                                  const std::string& key,
                                  bool success,
                                  std::unique_ptr<StrikeData> strike_data) {
  if (success && strike_data) {
    outer_callback.Run(strike_data->num_strikes());
    UpdateCache(key, *strike_data.get());
  } else {
    outer_callback.Run(0);
  }
}

void StrikeDatabase::OnAddStrike(StrikesCallback outer_callback,
                                 const std::string& key,
                                 bool success,
                                 std::unique_ptr<StrikeData> strike_data) {
  if (!success) {
    outer_callback.Run(0);
    return;
  }
  int num_strikes = strike_data ? strike_data->num_strikes() + 1 : 1;
  std::unique_ptr<StrikeData> data = std::make_unique<StrikeData>(StrikeData());
  data->set_num_strikes(num_strikes);
  data->set_last_update_timestamp(
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  SetStrikeData(key, *data.get(),
                base::BindRepeating(&StrikeDatabase::OnAddStrikeComplete,
                                    base::Unretained(this), outer_callback, key,
                                    *data.get()));
}

void StrikeDatabase::OnAddStrikeComplete(StrikesCallback outer_callback,
                                         const std::string& key,
                                         const StrikeData& data,
                                         bool success) {
  if (success) {
    outer_callback.Run(data.num_strikes());
    UpdateCache(key, data);
  } else {
    outer_callback.Run(0);
  }
}

void StrikeDatabase::OnClearAllStrikesForKey(
    ClearStrikesCallback outer_callback,
    const std::string& key,
    bool success) {
  strike_map_cache_.erase(key);
  outer_callback.Run(success);
}

void StrikeDatabase::UpdateCache(const std::string& key,
                                 const StrikeData& data) {
  strike_map_cache_[key] = data;
}

std::string StrikeDatabase::CreateKey(const std::string& type_prefix,
                                      const std::string& identifier_suffix) {
  return type_prefix + kKeyDeliminator + identifier_suffix;
}

std::string StrikeDatabase::GetKeyPrefixForCreditCardSave() {
  return kKeyPrefixForCreditCardSave;
}

}  // namespace autofill
