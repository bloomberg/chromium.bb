// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"

#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {
namespace {

// Adapter from the leveldb::Slice comparator version to the base::StringPiece
// version.
class LevelDBComparatorToIDBComparator : public LevelDBComparator {
 public:
  explicit LevelDBComparatorToIDBComparator(
      const leveldb::Comparator* comparator)
      : comparator_(comparator) {}

  int Compare(const base::StringPiece& a,
              const base::StringPiece& b) const override {
    return comparator_->Compare(leveldb_env::MakeSlice(a),
                                leveldb_env::MakeSlice(b));
  }
  const char* Name() const override { return comparator_->Name(); }
  void FindShortestSeparator(std::string* start,
                             const base::StringPiece& limit) const override {
    comparator_->FindShortestSeparator(start, leveldb_env::MakeSlice(limit));
  }
  void FindShortSuccessor(std::string* key) const override {
    comparator_->FindShortSuccessor(key);
  }

 private:
  const leveldb::Comparator* comparator_;
};

}  // namespace

// static
const LevelDBComparator* LevelDBComparator::BytewiseComparator() {
  static const base::NoDestructor<LevelDBComparatorToIDBComparator>
      idb_comparator(leveldb::BytewiseComparator());
  return idb_comparator.get();
}

}  // namespace content
