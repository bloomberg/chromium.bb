// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_database_provider.h"

#include "base/files/file_path.h"

namespace leveldb_proto {

ProtoDatabaseProvider::ProtoDatabaseProvider(const base::FilePath& profile_dir)
    : profile_dir_(profile_dir) {}

// static
ProtoDatabaseProvider* ProtoDatabaseProvider::Create(
    const base::FilePath& profile_dir) {
  return new ProtoDatabaseProvider(profile_dir);
}

}  // namespace leveldb_proto
