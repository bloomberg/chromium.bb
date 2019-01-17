// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_
#define COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_

#include <stddef.h>

#include <string>

namespace leveldb_proto {

const char* const kFeatureEngagementName = "FeatureEngagement";

// NOTE: The client names should not have partial or complete prefix overlap
// with any other client name, current or obsolete. Internally the stored data
// is grouped by the prefix of client name. These names cannot be renamed
// without adding the old name to obsolete client list and such rename would
// make the client be treated as a new client.
const char* const kCurrentSharedProtoDatabaseClients[] = {
    kFeatureEngagementName, nullptr};

const char* const kObsoleteSharedProtoDatabaseClients[] = {
    nullptr  // Marks the last element.
};

class SharedProtoDatabaseClientList {
 public:
  static bool ShouldUseSharedDB(const std::string& client_name);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_PUBLIC_SHARED_PROTO_DATABASE_CLIENT_LIST_H_