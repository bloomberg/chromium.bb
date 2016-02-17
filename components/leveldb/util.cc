// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/util.h"

#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {

DatabaseError LeveldbStatusToError(const leveldb::Status& s) {
  if (s.ok())
    return DatabaseError::OK;
  if (s.IsNotFound())
    return DatabaseError::NOT_FOUND;
  if (s.IsCorruption())
    return DatabaseError::CORRUPTION;
  if (s.IsNotSupportedError())
    return DatabaseError::NOT_SUPPORTED;
  if (s.IsIOError())
    return DatabaseError::IO_ERROR;
  return DatabaseError::INVALID_ARGUMENT;
}

leveldb::Slice GetSliceFor(const mojo::Array<uint8_t>& key) {
  if (key.size() == 0)
    return leveldb::Slice();
  return leveldb::Slice(reinterpret_cast<const char*>(&key.front()),
                        key.size());
}

}  // namespace leveldb
