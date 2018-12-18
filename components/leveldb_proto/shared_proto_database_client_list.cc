// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database_client_list.h"

#include <stddef.h>

#include <string>

#include "base/stl_util.h"

namespace leveldb_proto {

const char kFeatureEngagementClientName[] = "FeatureEngagement";

const char* const kCurrentSharedProtoDatabaseClients[] = {
    kFeatureEngagementClientName,
};
const size_t kCurrentSharedProtoDatabaseClientsLength =
    base::size(kCurrentSharedProtoDatabaseClients);

#ifdef SHARED_PROTO_DATABASE_CLIENT_LIST_USE_OBSOLETE_CLIENT_LIST
const char* const kObsoleteSharedProtoDatabaseClients[] = {
    /* Add obsolete clients here. */
};
const size_t kObsoleteSharedProtoDatabaseClientsLength =
    base::size(kObsoleteSharedProtoDatabaseClients);
#endif  // SHARED_PROTO_DATABASE_CLIENT_LIST_USE_OBSOLETE_CLIENT_LIST

// static
bool SharedProtoDatabaseClientList::ShouldUseSharedDB(
    const std::string& client_name) {
  // TODO(thildebr): Make this check variations flags instead of just returning
  // false.
  return false;
}

}  // namespace leveldb_proto