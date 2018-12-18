// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_CLIENT_LIST_H_
#define COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_CLIENT_LIST_H_

#include <stddef.h>

#include <string>

namespace leveldb_proto {

extern const char kFeatureEngagementClientName[];

extern const char* const kCurrentSharedProtoDatabaseClients[];
extern const size_t kCurrentSharedProtoDatabaseClientsLength;

#ifdef SHARED_PROTO_DATABASE_CLIENT_LIST_USE_OBSOLETE_CLIENT_LIST
extern const char* const kObsoleteSharedProtoDatabaseClients[];
extern const size_t kObsoleteSharedProtoDatabaseClientsLength;
#endif  // SHARED_PROTO_DATABASE_CLIENT_LIST_USE_OBSOLETE_CLIENT_LIST

class SharedProtoDatabaseClientList {
 public:
  static bool ShouldUseSharedDB(const std::string& client_name);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_SHARED_PROTO_DATABASE_CLIENT_LIST_H_