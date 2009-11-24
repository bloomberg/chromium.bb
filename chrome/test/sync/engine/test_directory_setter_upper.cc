// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/sync/engine/test_directory_setter_upper.h"

#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/util/event_sys-inl.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::DirectoryManager;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;

namespace browser_sync {

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_("Test") {}
TestDirectorySetterUpper::TestDirectorySetterUpper(const std::string& name)
    : name_(name) {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::Init() {
  FilePath test_data_dir_(FILE_PATH_LITERAL("."));
  manager_.reset(new DirectoryManager(test_data_dir_));
  file_path_ = manager_->GetSyncDataDatabasePath();
  file_util::Delete(file_path_, false);
}

void TestDirectorySetterUpper::SetUp() {
  Init();
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
  EXPECT_TRUE(file_util::Delete(file_path_, false));
  file_path_ = FilePath(FILE_PATH_LITERAL(""));
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

void ManuallyOpenedTestDirectorySetterUpper::SetUp() {
  Init();
}

void ManuallyOpenedTestDirectorySetterUpper::Open() {
  ASSERT_TRUE(manager()->Open(name()));
  was_opened_ = true;
}

void ManuallyOpenedTestDirectorySetterUpper::TearDown() {
  if (was_opened_) {
    TestDirectorySetterUpper::TearDown();
  }
}

TriggeredOpenTestDirectorySetterUpper::TriggeredOpenTestDirectorySetterUpper(
    const std::string& name)
    : TestDirectorySetterUpper(name) {
}

void TriggeredOpenTestDirectorySetterUpper::SetUp() {
  Init();
}

void TriggeredOpenTestDirectorySetterUpper::TearDown() {
  DirectoryManager::DirNames names;
  manager()->GetOpenDirectories(&names);
  if (!names.empty()) {
    ASSERT_EQ(1U, names.size());
    ASSERT_EQ(name(), names[0]);
    TestDirectorySetterUpper::TearDown();
  }
}

}  // namespace browser_sync
