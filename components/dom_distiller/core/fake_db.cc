// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/fake_db.h"

#include "base/bind.h"

namespace dom_distiller {
namespace test {

FakeDB::FakeDB(EntryMap* db) : db_(db) {}

FakeDB::~FakeDB() {}

void FakeDB::Init(const base::FilePath& database_dir,
                  DomDistillerDatabaseInterface::InitCallback callback) {
  dir_ = database_dir;
  init_callback_ = callback;
}

void FakeDB::UpdateEntries(
    scoped_ptr<EntryVector> entries_to_save,
    scoped_ptr<EntryVector> entries_to_remove,
    DomDistillerDatabaseInterface::UpdateCallback callback) {
  for (EntryVector::iterator it = entries_to_save->begin();
       it != entries_to_save->end();
       ++it) {
    (*db_)[it->entry_id()] = *it;
  }
  for (EntryVector::iterator it = entries_to_remove->begin();
       it != entries_to_remove->end();
       ++it) {
    (*db_).erase(it->entry_id());
  }
  update_callback_ = callback;
}

void FakeDB::LoadEntries(DomDistillerDatabaseInterface::LoadCallback callback) {
  scoped_ptr<EntryVector> entries(new EntryVector());
  for (EntryMap::iterator it = db_->begin(); it != db_->end(); ++it) {
    entries->push_back(it->second);
  }
  load_callback_ =
      base::Bind(RunLoadCallback, callback, base::Passed(&entries));
}

base::FilePath& FakeDB::GetDirectory() { return dir_; }

void FakeDB::InitCallback(bool success) {
  init_callback_.Run(success);
  init_callback_.Reset();
}

void FakeDB::LoadCallback(bool success) {
  load_callback_.Run(success);
  load_callback_.Reset();
}

void FakeDB::UpdateCallback(bool success) {
  update_callback_.Run(success);
  update_callback_.Reset();
}

// static
void FakeDB::RunLoadCallback(
    DomDistillerDatabaseInterface::LoadCallback callback,
    scoped_ptr<EntryVector> entries,
    bool success) {
  callback.Run(success, entries.Pass());
}

// static
base::FilePath FakeDB::DirectoryForTestDB() {
  return base::FilePath(FILE_PATH_LITERAL("/fake/path"));
}

}  // namespace test
}  // namespace dom_distiller
