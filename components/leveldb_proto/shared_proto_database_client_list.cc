// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/shared_proto_database_client_list.h"

#include <stddef.h>

#include <string>

#include "base/stl_util.h"

namespace leveldb_proto {

// static
bool SharedProtoDatabaseClientList::ShouldUseSharedDB(
    const std::string& client_name) {
  // TODO(thildebr): Make this check variations flags instead of just returning
  // false.
  return false;
}

}  // namespace leveldb_proto