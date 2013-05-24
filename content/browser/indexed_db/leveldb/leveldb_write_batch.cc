// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_write_batch.h"

#include "content/browser/indexed_db/leveldb/leveldb_slice.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace content {

scoped_ptr<LevelDBWriteBatch> LevelDBWriteBatch::Create() {
  return make_scoped_ptr(new LevelDBWriteBatch);
}

LevelDBWriteBatch::LevelDBWriteBatch()
    : write_batch_(new leveldb::WriteBatch) {}

LevelDBWriteBatch::~LevelDBWriteBatch() {}

static leveldb::Slice MakeSlice(const LevelDBSlice& s) {
  return leveldb::Slice(s.begin(), s.end() - s.begin());
}

void LevelDBWriteBatch::Put(const LevelDBSlice& key,
                            const LevelDBSlice& value) {
  write_batch_->Put(MakeSlice(key), MakeSlice(value));
}

void LevelDBWriteBatch::Remove(const LevelDBSlice& key) {
  write_batch_->Delete(MakeSlice(key));
}

void LevelDBWriteBatch::Clear() { write_batch_->Clear(); }

}  // namespace content
