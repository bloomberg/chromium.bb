// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace usage_stats {

using leveldb_proto::ProtoDatabaseProvider;
using leveldb_proto::ProtoDatabaseProviderFactory;

const char kNamespace[] = "usage_stats";

const char kKeySeparator[] = "_";

const char kWebsiteEventPrefix[] = "website_event";
const char kSuspensionPrefix[] = "suspension";
const char kTokenMappingPrefix[] = "token_mapping";

const int kUnixTimeDigits = 11;
// Formats an integer with a minimum width of 11, right-justified, and
// zero-filled (example: 1548353315 => 01548353315).
const char kUnixTimeFormat[] = "%011d";

UsageStatsDatabase::UsageStatsDatabase(Profile* profile)
    : weak_ptr_factory_(this) {
  ProtoDatabaseProvider* db_provider =
      ProtoDatabaseProviderFactory::GetInstance()->GetForBrowserContext(
          profile);

  base::FilePath usage_stats_dir = profile->GetPath().Append(kNamespace);

  scoped_refptr<base::SequencedTaskRunner> db_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  website_event_db_ = db_provider->GetDB<WebsiteEvent>(
      kNamespace, kWebsiteEventPrefix, usage_stats_dir, db_task_runner);

  suspension_db_ = db_provider->GetDB<Suspension>(
      kNamespace, kSuspensionPrefix, usage_stats_dir, db_task_runner);

  token_mapping_db_ = db_provider->GetDB<TokenMapping>(
      kNamespace, kTokenMappingPrefix, usage_stats_dir, db_task_runner);
}

// Used for testing.
UsageStatsDatabase::UsageStatsDatabase(
    std::unique_ptr<ProtoDatabase<WebsiteEvent>> website_event_db,
    std::unique_ptr<ProtoDatabase<Suspension>> suspension_db,
    std::unique_ptr<ProtoDatabase<TokenMapping>> token_mapping_db)
    : website_event_db_(std::move(website_event_db)),
      suspension_db_(std::move(suspension_db)),
      token_mapping_db_(std::move(token_mapping_db)),
      weak_ptr_factory_(this) {}

UsageStatsDatabase::~UsageStatsDatabase() = default;

namespace {

bool DoesNotContainFilter(const base::flat_set<std::string>& set,
                          const std::string& key) {
  return !set.contains(key);
}

bool KeyContainsDomainFilter(const base::flat_set<std::string>& domains,
                             const std::string& key) {
  return domains.contains(key.substr(kUnixTimeDigits + 1));
}

UsageStatsDatabase::Error ToError(bool isSuccess) {
  return isSuccess ? UsageStatsDatabase::Error::kNoError
                   : UsageStatsDatabase::Error::kUnknownError;
}

std::string CreateWebsiteEventKey(int64_t seconds, const std::string& fqdn) {
  // Zero-pad the Unix time in seconds since epoch. Allows ascending timestamps
  // to sort lexicographically, supporting efficient range queries by key.
  char unixTime[kUnixTimeDigits + 1];
  ssize_t printed =
      base::strings::SafeSPrintf(unixTime, kUnixTimeFormat, seconds);
  DCHECK(printed == kUnixTimeDigits);

  // Create the key from the time and fqdn (example: 01548276551_foo.com).
  return base::StrCat({unixTime, kKeySeparator, fqdn});
}

}  // namespace

void UsageStatsDatabase::GetAllEvents(EventsCallback callback) {
  // Load all WebsiteEvents.
  website_event_db_->LoadEntries(
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllEvents,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::QueryEventsInRange(int64_t start,
                                            int64_t end,
                                            EventsCallback callback) {
  // Load all WebsiteEvents where the timestamp is
  // in the specified range. Function accepts a half-open range [start, end) as
  // input, but the database operates on fully-closed ranges. Because the
  // timestamps are represented by integers, [start, end) is equivalent to
  // [start, end - 1].
  website_event_db_->LoadKeysAndEntriesInRange(
      CreateWebsiteEventKey(start, ""), CreateWebsiteEventKey(end - 1, ""),
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForQueryEventsInRange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::AddEvents(std::vector<WebsiteEvent> events,
                                   StatusCallback callback) {
  auto entries =
      std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>();
  entries->reserve(events.size());

  for (WebsiteEvent event : events) {
    std::string key =
        CreateWebsiteEventKey(event.timestamp().seconds(), event.fqdn());

    entries->emplace_back(key, event);
  }

  // Add all entries created from input vector.
  website_event_db_->UpdateEntries(
      std::move(entries), std::make_unique<std::vector<std::string>>(),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::DeleteAllEvents(StatusCallback callback) {
  // Remove all WebsiteEvent entries.
  website_event_db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>(),
      base::BindRepeating([](const std::string& key) { return true; }),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
void UsageStatsDatabase::DeleteEventsInRange(int64_t start,
                                             int64_t end,
                                             StatusCallback callback) {
  // TODO(crbug/927655): If/when leveldb_proto adds an UpdateEntriesInRange
  // function, we should consolidate these two proto_db_ calls into a single
  // call.

  // Load all keys where the timestamp is in the specified range, passing a
  // callback to delete those entries. Function accepts a half-open range
  // [start, end) as input, but the database operates on fully-closed ranges.
  // Because the timestamps are represented by integers, [start, end) is
  // equivalent to [start, end - 1].
  website_event_db_->LoadKeysAndEntriesInRange(
      CreateWebsiteEventKey(start, ""), CreateWebsiteEventKey(end - 1, ""),
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForDeleteEventsInRange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::DeleteEventsWithMatchingDomains(
    base::flat_set<std::string> domains,
    StatusCallback callback) {
  // Remove all events with domains in the given set.
  website_event_db_->UpdateEntriesWithRemoveFilter(
      std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>(),
      base::BindRepeating(&KeyContainsDomainFilter, std::move(domains)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::GetAllSuspensions(SuspensionsCallback callback) {
  // Load all Suspensions.
  suspension_db_->LoadEntries(
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::SetSuspensions(base::flat_set<std::string> domains,
                                        StatusCallback callback) {
  auto entries = std::make_unique<ProtoDatabase<Suspension>::KeyEntryVector>();

  for (std::string domain : domains) {
    Suspension suspension;
    suspension.set_fqdn(domain);

    entries->emplace_back(domain, suspension);
  }

  // Add all entries created from domain set, remove all entries not in the set.
  suspension_db_->UpdateEntriesWithRemoveFilter(
      std::move(entries),
      base::BindRepeating(&DoesNotContainFilter, std::move(domains)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::GetAllTokenMappings(TokenMappingsCallback callback) {
  // Load all TokenMappings.
  token_mapping_db_->LoadEntries(
      base::BindOnce(&UsageStatsDatabase::OnLoadEntriesForGetAllTokenMappings,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::SetTokenMappings(TokenMap mappings,
                                          StatusCallback callback) {
  std::vector<std::string> keys;
  keys.reserve(mappings.size());

  auto entries =
      std::make_unique<ProtoDatabase<TokenMapping>::KeyEntryVector>();

  for (const auto& mapping : mappings) {
    std::string token = mapping.first;
    std::string fqdn = mapping.second;

    keys.emplace_back(token);

    TokenMapping token_mapping;
    token_mapping.set_token(token);
    token_mapping.set_fqdn(fqdn);

    entries->emplace_back(token, token_mapping);
  }

  auto key_set = base::flat_set<std::string>(keys);

  // Add all entries created from map, remove all entries not in the map.
  token_mapping_db_->UpdateEntriesWithRemoveFilter(
      std::move(entries),
      base::BindRepeating(&DoesNotContainFilter, std::move(key_set)),
      base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UsageStatsDatabase::OnUpdateEntries(StatusCallback callback,
                                         bool isSuccess) {
  std::move(callback).Run(ToError(isSuccess));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllEvents(
    EventsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::vector<WebsiteEvent>> events) {
  if (isSuccess && events) {
    std::move(callback).Run(ToError(isSuccess), *events);
  } else {
    std::move(callback).Run(ToError(isSuccess), std::vector<WebsiteEvent>());
  }
}

void UsageStatsDatabase::OnLoadEntriesForQueryEventsInRange(
    EventsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::map<std::string, WebsiteEvent>> event_map) {
  std::vector<WebsiteEvent> results;

  if (event_map) {
    results.reserve(event_map->size());
    for (const auto& entry : *event_map) {
      results.emplace_back(entry.second);
    }
  }

  std::move(callback).Run(ToError(isSuccess), std::move(results));
}

void UsageStatsDatabase::OnLoadEntriesForDeleteEventsInRange(
    StatusCallback callback,
    bool isSuccess,
    std::unique_ptr<std::map<std::string, WebsiteEvent>> event_map) {
  if (isSuccess && event_map) {
    // Collect keys found in range to be deleted.
    auto keys_to_delete = std::make_unique<std::vector<std::string>>();
    keys_to_delete->reserve(event_map->size());

    for (const auto& entry : *event_map) {
      keys_to_delete->emplace_back(entry.first);
    }

    // Remove all entries found in range.
    website_event_db_->UpdateEntries(
        std::make_unique<ProtoDatabase<WebsiteEvent>::KeyEntryVector>(),
        std::move(keys_to_delete),
        base::BindOnce(&UsageStatsDatabase::OnUpdateEntries,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    std::move(callback).Run(ToError(isSuccess));
  }
}

void UsageStatsDatabase::OnLoadEntriesForGetAllSuspensions(
    SuspensionsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::vector<Suspension>> suspensions) {
  std::vector<std::string> results;

  if (suspensions) {
    results.reserve(suspensions->size());
    for (Suspension suspension : *suspensions) {
      results.emplace_back(suspension.fqdn());
    }
  }

  std::move(callback).Run(ToError(isSuccess), std::move(results));
}

void UsageStatsDatabase::OnLoadEntriesForGetAllTokenMappings(
    TokenMappingsCallback callback,
    bool isSuccess,
    std::unique_ptr<std::vector<TokenMapping>> mappings) {
  TokenMap results;

  if (mappings) {
    for (TokenMapping mapping : *mappings) {
      results.emplace(mapping.token(), mapping.fqdn());
    }
  }

  std::move(callback).Run(ToError(isSuccess), std::move(results));
}

}  // namespace usage_stats
