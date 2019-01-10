// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include "base/task/post_task.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"
#include "components/leveldb_proto/proto_database_provider.h"

namespace usage_stats {

const char kNamespace[] = "usage_stats";
const char kWebsiteEventPrefix[] = "website_event";
const char kSuspensionPrefix[] = "suspension";
const char kTokenMappingPrefix[] = "token_mapping";

UsageStatsDatabase::UsageStatsDatabase(Profile* profile)
    : weak_ptr_factory_(this) {
  leveldb_proto::ProtoDatabaseProvider* db_provider =
      leveldb_proto::ProtoDatabaseProviderFactory::GetInstance()
          ->GetForBrowserContext(profile);

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

UsageStatsDatabase::~UsageStatsDatabase() = default;

}  // namespace usage_stats
