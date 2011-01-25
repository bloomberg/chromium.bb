// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_worker.h"

#include "base/win/registry.h"
#include "base/version.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/work_item_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using installer::InstallationState;
using installer::InstallerState;
using installer::ProductState;

using ::testing::_;
using ::testing::AtLeast;

// Mock classes to help with testing
//------------------------------------------------------------------------------

class MockWorkItemList : public WorkItemList {
 public:
  MockWorkItemList() {}

  MOCK_METHOD5(AddCopyTreeWorkItem, WorkItem*(const std::wstring&,
                                              const std::wstring&,
                                              const std::wstring&,
                                              CopyOverWriteOption,
                                              const std::wstring&));
  MOCK_METHOD1(AddCreateDirWorkItem, WorkItem* (const FilePath&));
  MOCK_METHOD2(AddCreateRegKeyWorkItem, WorkItem* (HKEY, const std::wstring&));
  MOCK_METHOD2(AddDeleteRegKeyWorkItem, WorkItem* (HKEY, const std::wstring&));
  MOCK_METHOD4(AddDeleteRegValueWorkItem, WorkItem* (HKEY,
                                                     const std::wstring&,
                                                     const std::wstring&,
                                                     bool));
  MOCK_METHOD2(AddDeleteTreeWorkItem, WorkItem* (const FilePath&,
                                                 const std::vector<FilePath>&));
  MOCK_METHOD1(AddDeleteTreeWorkItem, WorkItem* (const FilePath&));
  MOCK_METHOD3(AddMoveTreeWorkItem, WorkItem* (const std::wstring&,
                                               const std::wstring&,
                                               const std::wstring&));
  MOCK_METHOD5(AddSetRegValueWorkItem, WorkItem*(HKEY,
                                                 const std::wstring&,
                                                 const std::wstring&,
                                                 const std::wstring&,
                                                 bool));
  MOCK_METHOD5(AddSetRegValueWorkItem, WorkItem* (HKEY,
                                                  const std::wstring&,
                                                  const std::wstring&,
                                                  DWORD,
                                                  bool));
  MOCK_METHOD3(AddSelfRegWorkItem, WorkItem* (const std::wstring&,
                                              bool,
                                              bool));
};

class MockProductState : public ProductState {
 public:
  // Takes ownership of |version|.
  void set_version(Version* version) { version_.reset(version); }
};

// Okay, so this isn't really a mock as such, but it does add setter methods
// to make it easier to build custom InstallationStates.
class MockInstallationState : public InstallationState {
 public:
  // Included for testing.
  void SetProductState(bool system_install,
                       BrowserDistribution::Type type,
                       const ProductState& product_state) {
    ProductState& target = (system_install ? system_products_ :
        user_products_)[IndexFromDistType(type)];
    target.CopyFrom(product_state);
  }
};

class MockInstallerState : public InstallerState {
 public:
  void set_level(Level level) {
    level_ = level;
  }

  void set_operation(Operation operation) { operation_ = operation; }

  void set_state_key(const std::wstring& state_key) {
    state_key_ = state_key;
  }
};

// The test fixture
//------------------------------------------------------------------------------

class InstallWorkerTest : public testing::Test {
 public:
  virtual void SetUp() {
    current_version_.reset(Version::GetVersionFromString("1.0.0.0"));
    new_version_.reset(Version::GetVersionFromString("42.0.0.0"));

    // Don't bother ensuring that these paths exist. Since we're just
    // building the work item lists and not running them, they shouldn't
    // actually be touched.
    archive_path_ = FilePath(L"C:\\UnlikelyPath\\Temp\\chrome_123\\chrome.7z");
    // TODO(robertshield): Take this from the BrowserDistribution once that
    // no longer depends on MasterPreferences.
    installation_path_ = FilePath(L"C:\\Program Files\\Google\\Chrome\\");
    src_path_ =
        FilePath(L"C:\\UnlikelyPath\\Temp\\chrome_123\\source\\Chrome-bin");
    setup_path_ = FilePath(L"C:\\UnlikelyPath\\Temp\\CR_123.tmp\\setup.exe");
    temp_dir_ = FilePath(L"C:\\UnlikelyPath\\Temp\\chrome_123");
  }

  virtual void TearDown() {
  }

  MockInstallationState* BuildChromeSingleSystemInstallationState() {
    scoped_ptr<MockInstallationState> installation_state(
        new MockInstallationState());

    MockProductState product_state;
    product_state.set_version(current_version_->Clone());
    // Do not call SetMultiPackageState since this is a single install.
    installation_state->SetProductState(true,
                                        BrowserDistribution::CHROME_BROWSER,
                                        product_state);

    return installation_state.release();
  }

  MockInstallerState* BuildChromeSingleSystemInstallerState(
      const InstallationState& machine_state) {
    scoped_ptr<MockInstallerState> installer_state(new MockInstallerState());

    installer_state->set_level(InstallerState::SYSTEM_LEVEL);
    installer_state->set_operation(InstallerState::SINGLE_INSTALL_OR_UPDATE);
    // Hope this next one isn't checked for now.
    installer_state->set_state_key(L"PROBABLY_INVALID_REG_PATH");
    const ProductState* chrome =
        machine_state.GetProductState(true,
                                      BrowserDistribution::CHROME_BROWSER);
    installer_state->AddProductFromState(BrowserDistribution::CHROME_BROWSER,
                                         *chrome);
    return installer_state.release();
  }

 protected:
  scoped_ptr<Version> current_version_;
  scoped_ptr<Version> new_version_;
  FilePath archive_path_;
  FilePath installation_path_;
  FilePath setup_path_;
  FilePath src_path_;
  FilePath temp_dir_;
};

// Tests
//------------------------------------------------------------------------------

TEST_F(InstallWorkerTest, TestInstallChromeSingleSystem) {
  MockWorkItemList work_item_list;

  scoped_ptr<InstallationState> installation_state(
      BuildChromeSingleSystemInstallationState());

  scoped_ptr<InstallerState> installer_state(
      BuildChromeSingleSystemInstallerState(*installation_state));

  // Set up some expectations.
  // TODO(robertshield): Set up some real expectations.
  EXPECT_CALL(work_item_list, AddCopyTreeWorkItem(_,_,_,_,_))
      .Times(AtLeast(1));

  AddInstallWorkItems(*installation_state.get(),
                      *installer_state.get(),
                      setup_path_,
                      archive_path_,
                      src_path_,
                      temp_dir_,
                      *new_version_.get(),
                      &current_version_,
                      &work_item_list);
}
