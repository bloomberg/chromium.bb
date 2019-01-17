// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_wrapper.h"

#include "components/leveldb_proto/public/proto_database_provider.h"

namespace leveldb_proto {

void GetSharedDBInstance(
    ProtoDatabaseProvider* db_provider,
    base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)> callback) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  db_provider->GetSharedDBInstance(std::move(callback),
                                   base::SequencedTaskRunnerHandle::Get());
}

}  // namespace leveldb_proto