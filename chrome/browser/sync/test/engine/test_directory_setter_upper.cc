// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/engine/test_directory_setter_upper.h"

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/string_util.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/null_transaction_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncable::DirectoryManager;
using syncable::NullTransactionObserver;
using syncable::ReadTransaction;
using syncable::ScopedDirLookup;

namespace browser_sync {

TestDirectorySetterUpper::TestDirectorySetterUpper() : name_("Test") {}
TestDirectorySetterUpper::TestDirectorySetterUpper(const std::string& name)
    : name_(name) {}

TestDirectorySetterUpper::~TestDirectorySetterUpper() {}

void TestDirectorySetterUpper::Init() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  reset_directory_manager(new DirectoryManager(temp_dir_.path()));
  // There shouldn't be any existing database in the newly-created
  // temp dir.
  ASSERT_FALSE(file_util::PathExists(manager_->GetSyncDataDatabasePath()));
}

void TestDirectorySetterUpper::reset_directory_manager(DirectoryManager* d) {
  manager_.reset(d);
}

void TestDirectorySetterUpper::SetUp() {
  Init();
  ASSERT_TRUE(manager()->Open(name(), &delegate_, NullTransactionObserver()));
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
  ASSERT_TRUE(temp_dir_.Delete());
}

void TestDirectorySetterUpper::RunInvariantCheck(const ScopedDirLookup& dir) {
  {
    // Check invariants for in-memory items.
    ReadTransaction trans(FROM_HERE, dir);
    dir->CheckTreeInvariants(&trans, false);
  }
  {
    // Check invariants for all items.
    ReadTransaction trans(FROM_HERE, dir);
    dir->CheckTreeInvariants(&trans, true);
  }
}

void ManuallyOpenedTestDirectorySetterUpper::SetUp() {
  Init();
}

void ManuallyOpenedTestDirectorySetterUpper::Open() {
  ASSERT_TRUE(
      manager()->Open(name(), &delegate_, NullTransactionObserver()));
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

MockDirectorySetterUpper::MockDirectory::MockDirectory(
    const std::string& name) {
  InitKernelForTest(name, &delegate_, NullTransactionObserver());
}

MockDirectorySetterUpper::MockDirectory::~MockDirectory() {}

MockDirectorySetterUpper::Manager::Manager(
    const FilePath& root_path, syncable::Directory* dir) :
    syncable::DirectoryManager(root_path) {
  managed_directory_ = dir;
}

MockDirectorySetterUpper::MockDirectorySetterUpper()
    : directory_(new MockDirectory(name())) {
}

MockDirectorySetterUpper::~MockDirectorySetterUpper() {}

void MockDirectorySetterUpper::SetUp() {
  reset_directory_manager(new Manager(FilePath(), directory_.get()));
}

void MockDirectorySetterUpper::TearDown() {
  // Nothing to do here.
}

}  // namespace browser_sync
