// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/shared_proto_database_client_list.h"

#include <stddef.h>

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"

#include "components/leveldb_proto/internal/leveldb_proto_feature_list.h"

namespace leveldb_proto {

const char* const kDBNameParamPrefix = "migrate_";

// static
bool SharedProtoDatabaseClientList::ShouldUseSharedDB(
    const std::string& client_name) {
  if (!base::FeatureList::IsEnabled(kProtoDBSharedMigration))
    return false;

  std::map<std::string, std::string> params;
  if (!base::GetFieldTrialParamsByFeature(kProtoDBSharedMigration, &params))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kProtoDBSharedMigration, kDBNameParamPrefix + client_name, false);
}

}  // namespace leveldb_proto