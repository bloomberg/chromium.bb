// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include <utility>

#include "base/task/post_task.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace usage_stats {

const char kNamespace[] = "usage_stats";
const char kTypePrefix[] = "usage_stats";

const char kKeySeparator = '.';
const char kSuspensionPrefix[] = "suspension";

UsageStatsDatabase::UsageStatsDatabase(Profile* profile)
    : weak_ptr_factory_(this) {
  leveldb_proto::ProtoDatabaseProvider* db_provider =
      leveldb_proto::ProtoDatabaseProviderFactory::GetInstance()
          ->GetForBrowserContext(profile);

  base::FilePath usage_stats_dir = profile->GetPath().Append(kNamespace);

  // TODO(crbug/921133): Switch back to separate dbs for each message type when
  // possible.
  proto_db_ = db_provider->GetDB<UsageStat>(
      kNamespace, kTypePrefix, usage_stats_dir,
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
}

UsageStatsDatabase::UsageStatsDatabase(
    std::unique_ptr<leveldb_proto::ProtoDatabase<UsageStat>> proto_db)
    : proto_db_(std::move(proto_db)), weak_ptr_factory_(this) {}

UsageStatsDatabase::~UsageStatsDatabase() = default;

namespace {
bool DoesNotContainFilter(const base::flat_set<std::string>& set,
                          const std::string& key) {
  return !set.contains(key);
}

UsageStatsDatabase::Error ToError(bool success) {
  return success ? UsageStatsDatabase::Error::kNoError
                 : UsageStatsDatabase::Error::kUnknownError;
}
}  // namespace

void UsageStatsDatabase::GetAllSuspensions(SuspensionCallback callback) {
  // Load all UsageStats with the suspension prefix.
  proto_db_->LoadEntriesWithFilter(
      leveldb_proto::LevelDB::KeyFilter(), leveldb::ReadOptions(),
      kSuspensionPrefix,
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::SetSuspensions(base::flat_set<std::string> domains,
                                        StatusCallback callback) {
  std::vector<std::string> keys;
  keys.reserve(domains.size());

  auto entries = std::make_unique<
      leveldb_proto::ProtoDatabase<UsageStat>::KeyEntryVector>();

  for (std::string domain : domains) {
    // Prepend prefix to form key.
    std::string key = std::string(kSuspensionPrefix) + kKeySeparator + domain;

    keys.emplace_back(key);

    UsageStat value;
    Suspension* suspension = value.mutable_suspension();
    suspension->set_fqdn(domain);

    entries->emplace_back(key, value);
  }

  auto key_set = base::flat_set<std::string>(keys);

  // Add all entries created from domain set, remove all entries not in the set.
  proto_db_->UpdateEntriesWithRemoveFilter(
      std::move(entries),
      base::BindRepeating(&DoesNotContainFilter, std::move(key_set)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntriesForSetSuspensions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions(
    SuspensionCallback callback,
    bool success,
    std::unique_ptr<std::vector<UsageStat>> stats) {
  std::vector<std::string> results;

  if (stats) {
    results.reserve(stats->size());
    for (UsageStat stat : *stats) {
      results.emplace_back(stat.suspension().fqdn());
    }
  }

  std::move(callback).Run(ToError(success), std::move(results));
}

void UsageStatsDatabase::OnUpdateEntriesForSetSuspensions(
    StatusCallback callback,
    bool success) {
  std::move(callback).Run(ToError(success));
}

}  // namespace usage_stats
