// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_UTIL_H_
#define COMPONENTS_LEVELDB_UTIL_H_

#include "components/leveldb/public/interfaces/leveldb.mojom.h"

namespace leveldb {

class Slice;
class Status;

DatabaseError LeveldbStatusToError(const leveldb::Status& s);

// Builds a Slice pointing to the data inside |a|. This is not a type-converter
// as it is not a copy operation; the returned Slice points into |a| and must
// outlive |a|.
leveldb::Slice GetSliceFor(const mojo::Array<uint8_t>& a);

}  // namespace leveldb

#endif  // COMPONENTS_LEVELDB_UTIL_H_
