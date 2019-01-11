// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/usage_stats/usage_stats_database.h"

#include "base/task/post_task.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"
#include "components/leveldb_proto/proto_database_provider.h"

namespace usage_stats {

const char kNamespace[] = "usage_stats";
const char kTypePrefix[] = "usage_stats";

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

UsageStatsDatabase::~UsageStatsDatabase() = default;

}  // namespace usage_stats
