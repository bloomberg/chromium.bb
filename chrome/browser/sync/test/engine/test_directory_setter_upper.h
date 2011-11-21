// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A handy class that takes care of setting up and destroying a
// syncable::Directory instance for unit tests that require one.
//
// The expected usage is to make this a component of your test fixture:
//
//   class AwesomenessTest : public testing::Test {
//    public:
//     virtual void SetUp() {
//       metadb_.SetUp();
//     }
//     virtual void TearDown() {
//       metadb_.TearDown();
//     }
//    protected:
//     TestDirectorySetterUpper metadb_;
//   };
//
// Then, in your tests, get at the directory like so:
//
//   TEST_F(AwesomenessTest, IsMaximal) {
//     ScopedDirLookup dir(metadb_.manager(), metadb_.name());
//     ... now use |dir| to get at syncable::Entry objects ...
//   }
//

#ifndef CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
#define CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/syncable/syncable.h"
#include "chrome/browser/sync/test/null_directory_change_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace syncable {
class DirectoryManager;
class ScopedDirLookup;
}  // namespace syncable

namespace browser_sync {

class TestDirectorySetterUpper {
 public:
  TestDirectorySetterUpper();
  virtual ~TestDirectorySetterUpper();

  // Create a DirectoryManager instance and use it to open the directory.
  // Clears any existing database backing files that might exist on disk.
  virtual void SetUp();

  // Undo everything done by SetUp(): close the directory and delete the
  // backing files. Before closing the directory, this will run the directory
  // invariant checks and perform the SaveChanges action on the directory.
  virtual void TearDown();

  syncable::DirectoryManager* manager() const { return manager_.get(); }
  const std::string& name() const { return name_; }

 protected:
  // Subclasses may want to use a different directory name.
  explicit TestDirectorySetterUpper(const std::string& name);
  virtual void Init();
  void reset_directory_manager(syncable::DirectoryManager* d);

  syncable::NullDirectoryChangeDelegate delegate_;

 private:
  void RunInvariantCheck(const syncable::ScopedDirLookup& dir);

  scoped_ptr<syncable::DirectoryManager> manager_;
  const std::string name_;
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(TestDirectorySetterUpper);
};

// A variant of the above where SetUp does not actually open the directory.
// You must manually invoke Open().  This is useful if you are writing a test
// that depends on the DirectoryManager::OPENED event.
class ManuallyOpenedTestDirectorySetterUpper : public TestDirectorySetterUpper {
 public:
  ManuallyOpenedTestDirectorySetterUpper() : was_opened_(false) {}
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  void Open();
 private:
  bool was_opened_;
};

// Use this flavor if you have a test that will trigger the opening event to
// happen automagically.  It is careful on teardown to close only if needed.
class TriggeredOpenTestDirectorySetterUpper : public TestDirectorySetterUpper {
 public:
  // A triggered open is typically in response to a successful auth event just
  // as in "real life".  In this case, the name that will be used should be
  // deterministically known at construction, and is passed in |name|.
  explicit TriggeredOpenTestDirectorySetterUpper(const std::string& name);
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
};

// Use this when you don't want to test the whole stack down to the Directory
// level, as it installs a google mock Directory implementation.
class MockDirectorySetterUpper : public TestDirectorySetterUpper {
 public:
  class Manager : public syncable::DirectoryManager {
   public:
    Manager(const FilePath& root_path, syncable::Directory* dir);
    virtual ~Manager() { managed_directory_ = NULL; }
  };

  class MockDirectory : public syncable::Directory {
   public:
    explicit MockDirectory(const std::string& name);
    virtual ~MockDirectory();
    MOCK_METHOD1(PurgeEntriesWithTypeIn, void(const syncable::ModelTypeSet&));

   private:
    syncable::NullDirectoryChangeDelegate delegate_;
  };

  MockDirectorySetterUpper();
  virtual ~MockDirectorySetterUpper();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  MockDirectory* directory() { return directory_.get(); }

 private:
  scoped_ptr<MockDirectory> directory_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_TEST_ENGINE_TEST_DIRECTORY_SETTER_UPPER_H_
