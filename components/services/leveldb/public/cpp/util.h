// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_LEVELDB_PUBLIC_CPP_UTIL_H_
#define COMPONENTS_SERVICES_LEVELDB_PUBLIC_CPP_UTIL_H_

#include "components/services/leveldb/public/interfaces/leveldb.mojom.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace leveldb {

class Slice;
class Status;

// Builds a mojo mojom::DatabaseError from a leveldb::Status object.
mojom::DatabaseError LeveldbStatusToError(const leveldb::Status& s);

// Creates a leveldb Status object form a database error and two optional
// messages. A mojoification of the various static leveldb::Status
// constructors.
leveldb::Status DatabaseErrorToStatus(mojom::DatabaseError e,
                                      const Slice& msg,
                                      const Slice& msg2);

// Returns an UMA value for a mojom::DatabaseError.
leveldb_env::LevelDBStatusValue GetLevelDBStatusUMAValue(
    mojom::DatabaseError status);

// Builds a Slice pointing to the data inside |a|. This is not a type-converter
// as it is not a copy operation; the returned Slice points into |a| and must
// outlive |a|.
leveldb::Slice GetSliceFor(const std::vector<uint8_t>& a);

// Copies the data that |s| points to into a std::vector.
std::vector<uint8_t> GetVectorFor(const leveldb::Slice& s);

std::string Uint8VectorToStdString(const std::vector<uint8_t>& input);

std::vector<uint8_t> StdStringToUint8Vector(const std::string& input);

}  // namespace leveldb

#endif  // COMPONENTS_SERVICES_LEVELDB_PUBLIC_CPP_UTIL_H_
