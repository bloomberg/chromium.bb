// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_

#include <stddef.h>

#include <string>

namespace leveldb_proto {

const char* const kFeatureEngagementName = "FeatureEngagement";

// The enum values are used to index into the shared database. Do not rearrange
// or reuse the integer values. Add new database types at the end of the enum,
// and update the string mapping in client_list.cc file.
enum class ProtoDbType {
  TEST_DATABASE0 = 0,
  TEST_DATABASE1 = 1,
  TEST_DATABASE2 = 2,
  FEATURE_ENGAGEMENT_EVENT = 3,
  FEATURE_ENGAGEMENT_AVAILABILITY = 4,
  USAGE_STATS_WEBSITE_EVENT = 5,
  USAGE_STATS_SUSPENSION = 6,
  USAGE_STATS_TOKEN_MAPPING = 7,

  LAST,
};

// List of databases that were introduced after shared db implementation was
// created. These will be forced to use shared database implementation.
constexpr ProtoDbType kWhitelistedDbForSharedImpl[]{
    ProtoDbType::LAST,  // Marks the end of list.
};

// Add any obsolete databases in this list so that, if the data is no longer
// needed.
constexpr ProtoDbType kObsoleteSharedProtoDbTypeClients[] = {
    ProtoDbType::LAST,  // Marks the end of list.
};

class SharedProtoDatabaseClientList {
 public:
  // Determines if the given |db_type| should use a unique or shared DB.
  static bool ShouldUseSharedDB(ProtoDbType db_type);

  // Converts a ProtoDbType to a string, which is used for UMA metrics and field
  // trials.
  static std::string ProtoDbTypeToString(ProtoDbType db_type);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_