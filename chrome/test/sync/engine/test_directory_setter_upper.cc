// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/sync/engine/test_directory_setter_upper.h"

#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/compat_file.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::DirectoryManager;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;

namespace browser_sync {

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_(PSTR("Test")) {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::SetUp() {
  PathString test_data_dir_ = PSTR(".");
  manager_.reset(new DirectoryManager(test_data_dir_));
  file_path_ = manager_->GetSyncDataDatabasePath();
  PathRemove(file_path_.c_str());

  ASSERT_TRUE(manager()->Open(name()));
}

void TestDirectorySetterUpper::TearDown() {
  if (!manager())
    return;

  {
    // A small scope so we don't have the dir open when we close it and reset
    // the DirectoryManager below.
    ScopedDirLookup dir(manager(), name());
    CHECK(dir.good()) << "Bad directory during tear down check";
    RunInvariantCheck(dir);
    dir->SaveChanges();
    RunInvariantCheck(dir);
    dir->SaveChanges();
  }

  manager()->FinalSaveChangesForAll();
  manager()->Close(name());
  manager_.reset();
  EXPECT_EQ(0, PathRemove(file_path_.c_str()));
  file_path_.clear();
}

void TestDirectorySetterUpper::RunInvariantCheck(const ScopedDirLookup& dir) {
  {
    // Check invariants for in-memory items.
    ReadTransaction trans(dir, __FILE__, __LINE__);
    dir->CheckTreeInvariants(&trans, false);
  }
  {
    // Check invariants for all items.
    ReadTransaction trans(dir, __FILE__, __LINE__);
    dir->CheckTreeInvariants(&trans, true);
  }
}

}  // namespace browser_sync
