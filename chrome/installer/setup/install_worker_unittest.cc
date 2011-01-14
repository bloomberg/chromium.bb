// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_worker.h"

#include "base/win/registry.h"
#include "base/version.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/work_item_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using installer::ChromiumPackageProperties;
using installer::InstallationState;
using installer::InstallerState;
using installer::Package;
using installer::PackageProperties;
using installer::PackagePropertiesImpl;
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

// Okay, so this isn't really a mock as such, but it does add setter methods
// to make it easier to build custom InstallationStates.
class MockInstallationState : public InstallationState {
 public:
  // Included for testing.
  void SetMultiPackageState(bool system_install,
                            const ProductState& package_state) {
    ProductState& target =
        (system_install ? system_products_ : user_products_)
            [MULTI_PACKAGE_INDEX];
    target.CopyFrom(package_state);
  }

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
  void set_system_install(bool system_install) {
    system_install_ = system_install;
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

    ProductState product_state;
    product_state.set_version(current_version_->Clone());
    // Do not call SetMultiPackageState since this is a single install.
    installation_state->SetProductState(true,
                                        BrowserDistribution::CHROME_BROWSER,
                                        product_state);

    return installation_state.release();
  }

  MockInstallerState* BuildChromeSingleSystemInstallerState() {
    scoped_ptr<MockInstallerState> installer_state(new MockInstallerState());

    installer_state->set_system_install(true);
    installer_state->set_operation(InstallerState::SINGLE_INSTALL_OR_UPDATE);
    // Hope this next one isn't checked for now.
    installer_state->set_state_key(L"PROBABLY_INVALID_REG_PATH");

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
      BuildChromeSingleSystemInstallerState());

  // This MUST outlive the package, since the package doesn't assume ownership
  // of it. Note: This feels like an implementation bug:
  // The PackageProperties <-> Package relationship is 1:1 and nothing else
  // uses the PackageProperties. I have the feeling that PackageProperties, and
  // perhaps Package itself should not exist at all.
  scoped_ptr<PackageProperties> package_properties(
      new ChromiumPackageProperties());

  scoped_refptr<Package> package(
      new Package(false, true, installation_path_, package_properties.get()));

  // Set up some expectations.
  // TODO(robertshield): Set up some real expectations.
  EXPECT_CALL(work_item_list, AddCopyTreeWorkItem(_,_,_,_,_))
      .Times(AtLeast(1));

  AddInstallWorkItems(*installation_state.get(),
                      *installer_state.get(),
                      false,
                      setup_path_,
                      archive_path_,
                      src_path_,
                      temp_dir_,
                      *new_version_.get(),
                      &current_version_,
                      *package.get(),
                      &work_item_list);
}
