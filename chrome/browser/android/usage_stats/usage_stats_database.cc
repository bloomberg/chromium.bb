// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include <utility>

#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace usage_stats {

const char kNamespace[] = "usage_stats";
const char kTypePrefix[] = "usage_stats";

const char kKeySeparator[] = "_";

const char kWebsiteEventPrefix[] = "website_event";
const char kSuspensionPrefix[] = "suspension";
const char kTokenMappingPrefix[] = "token_mapping";

const int kUnixTimeDigits = 11;
// Formats an integer with a minimum width of 11, right-justified, and
// zero-filled (example: 1548353315 => 01548353315).
const char kUnixTimeFormat[] = "%011d";

const int kFqdnPosition = base::size(kWebsiteEventPrefix) + kUnixTimeDigits + 1;

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

bool KeyContainsDomainFilter(const base::flat_set<std::string>& domains,
                             const std::string& key) {
  return domains.contains(key.substr(kFqdnPosition));
}

UsageStatsDatabase::Error ToError(bool success) {
  return success ? UsageStatsDatabase::Error::kNoError
                 : UsageStatsDatabase::Error::kUnknownError;
}

std::string CreateWebsiteEventKey(int64_t seconds, const std::string& fqdn) {
  // Zero-pad the Unix time in seconds since epoch. Allows ascending timestamps
  // to sort lexicographically, supporting efficient range queries by key.
  char unixTime[kUnixTimeDigits + 1];
  ssize_t printed =
      base::strings::SafeSPrintf(unixTime, kUnixTimeFormat, seconds);
  DCHECK(printed == kUnixTimeDigits);

  // Create the key from the prefix, time, and fqdn (example:
  // website_event_01548276551_foo.com).
  return base::StrCat(
      {kWebsiteEventPrefix, kKeySeparator, unixTime, kKeySeparator, fqdn});
}

}  // namespace

void UsageStatsDatabase::GetAllEvents(EventsCallback callback) {
  // Load all UsageStats with the website events prefix.
  proto_db_->LoadEntriesWithFilter(
      leveldb_proto::KeyFilter(), leveldb::ReadOptions(), kWebsiteEventPrefix,
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllEvents,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::AddEvents(std::vector<WebsiteEvent> events,
                                   StatusCallback callback) {
  auto entries = std::make_unique<
      leveldb_proto::ProtoDatabase<UsageStat>::KeyEntryVector>();
  entries->reserve(events.size());

  for (WebsiteEvent event : events) {
    std::string key =
        CreateWebsiteEventKey(event.timestamp().seconds(), event.fqdn());

    UsageStat value;
    WebsiteEvent* website_event = value.mutable_website_event();
    *website_event = event;

    entries->emplace_back(key, value);
  }

  // Add all entries created from input vector.
  proto_db_->UpdateEntries(
      std::move(entries), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::DeleteEventsWithMatchingDomains(
    base::flat_set<std::string> domains,
    StatusCallback callback) {
  // Remove all events with domains in the given set.
  proto_db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<
          leveldb_proto::ProtoDatabase<UsageStat>::KeyEntryVector>(),
      base::BindRepeating(&KeyContainsDomainFilter, std::move(domains)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::GetAllSuspensions(SuspensionsCallback callback) {
  // Load all UsageStats with the suspension prefix.
  proto_db_->LoadEntriesWithFilter(
      leveldb_proto::KeyFilter(), leveldb::ReadOptions(), kSuspensionPrefix,
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
    std::string key = base::StrCat({kSuspensionPrefix, kKeySeparator, domain});

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
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::GetAllTokenMappings(TokenMappingsCallback callback) {
  // Load all UsageStats with the token mapping prefix.
  proto_db_->LoadEntriesWithFilter(
      leveldb_proto::KeyFilter(), leveldb::ReadOptions(), kTokenMappingPrefix,
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllTokenMappings,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::SetTokenMappings(TokenMap mappings,
                                          StatusCallback callback) {
  std::vector<std::string> keys;
  keys.reserve(mappings.size());

  auto entries = std::make_unique<
      leveldb_proto::ProtoDatabase<UsageStat>::KeyEntryVector>();

  for (const auto& mapping : mappings) {
    // Prepend prefix to token to form key.
    std::string key =
        base::StrCat({kTokenMappingPrefix, kKeySeparator, mapping.first});

    keys.emplace_back(key);

    UsageStat value;
    TokenMapping* token_mapping = value.mutable_token_mapping();
    token_mapping->set_token(mapping.first);
    token_mapping->set_fqdn(mapping.second);

    entries->emplace_back(key, value);
  }

  auto key_set = base::flat_set<std::string>(keys);

  // Add all entries created from map, remove all entries not in the map.
  proto_db_->UpdateEntriesWithRemoveFilter(
      std::move(entries),
      base::BindRepeating(&DoesNotContainFilter, std::move(key_set)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::OnUpdateEntries(StatusCallback callback,
                                         bool success) {
  std::move(callback).Run(ToError(success));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllEvents(
    EventsCallback callback,
    bool success,
    std::unique_ptr<std::vector<UsageStat>> stats) {
  std::vector<WebsiteEvent> results;

  if (stats) {
    results.reserve(stats->size());
    for (UsageStat stat : *stats) {
      results.emplace_back(stat.website_event());
    }
  }

  std::move(callback).Run(ToError(success), std::move(results));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions(
    SuspensionsCallback callback,
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

void UsageStatsDatabase::OnLoadEntriesForGetAllTokenMappings(
    TokenMappingsCallback callback,
    bool success,
    std::unique_ptr<std::vector<UsageStat>> stats) {
  TokenMap results;

  if (stats) {
    for (UsageStat stat : *stats) {
      TokenMapping mapping = stat.token_mapping();
      results.emplace(mapping.token(), mapping.fqdn());
    }
  }

  std::move(callback).Run(ToError(success), std::move(results));
}

}  // namespace usage_stats
