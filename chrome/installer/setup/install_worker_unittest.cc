// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_worker.h"

#include "base/win/registry.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/util/delete_reg_key_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using installer::InstallationState;
using installer::InstallerState;
using installer::Product;
using installer::ProductState;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrCaseEq;
using ::testing::StrEq;
using ::testing::StrictMock;

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
  // Workaround for gmock problems with disambiguating between string pointers
  // and DWORD.
  virtual WorkItem* AddSetRegValueWorkItem(HKEY a1, const std::wstring& a2,
      const std::wstring& a3, const std::wstring& a4, bool a5) {
    return AddSetRegStringValueWorkItem(a1, a2, a3, a4, a5);
  }

  virtual WorkItem* AddSetRegValueWorkItem(HKEY a1, const std::wstring& a2,
                                           const std::wstring& a3,
                                           DWORD a4, bool a5) {
    return AddSetRegDwordValueWorkItem(a1, a2, a3, a4, a5);
  }

  MOCK_METHOD5(AddSetRegStringValueWorkItem, WorkItem*(HKEY,
                                                 const std::wstring&,
                                                 const std::wstring&,
                                                 const std::wstring&,
                                                 bool));
  MOCK_METHOD5(AddSetRegDwordValueWorkItem, WorkItem* (HKEY,
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
  void set_multi_install(bool multi) { multi_install_ = multi; }
  void SetUninstallProgram(const FilePath& setup_exe) {
    uninstall_command_ = CommandLine(setup_exe);
  }
  void AddUninstallSwitch(const std::string& option) {
    uninstall_command_.AppendSwitch(option);
  }
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
    InstallerState::set_level(level);
  }

  void set_operation(Operation operation) { operation_ = operation; }

  void set_state_key(const std::wstring& state_key) {
    state_key_ = state_key;
  }

  void set_package_type(PackageType type) {
    InstallerState::set_package_type(type);
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

  void AddChromeToInstallationState(
      bool system_level,
      bool multi_install,
      bool with_chrome_frame_ready_mode,
      MockInstallationState* installation_state) {
    MockProductState product_state;
    product_state.set_version(current_version_->Clone());
    product_state.set_multi_install(multi_install);
    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    FilePath install_path =
        installer::GetChromeInstallPath(system_level, dist);
    product_state.SetUninstallProgram(
        install_path.Append(installer::kSetupExe));
    product_state.AddUninstallSwitch(installer::switches::kUninstall);
    if (system_level)
      product_state.AddUninstallSwitch(installer::switches::kSystemLevel);
    if (multi_install) {
      product_state.AddUninstallSwitch(installer::switches::kMultiInstall);
      product_state.AddUninstallSwitch(installer::switches::kChrome);
      if (with_chrome_frame_ready_mode) {
        product_state.AddUninstallSwitch(installer::switches::kChromeFrame);
        product_state.AddUninstallSwitch(
            installer::switches::kChromeFrameReadyMode);
      }
    }

    installation_state->SetProductState(system_level,
                                        BrowserDistribution::CHROME_BROWSER,
                                        product_state);
  }

  void AddChromeFrameToInstallationState(
      bool system_level,
      bool multi_install,
      bool ready_mode,
      MockInstallationState* installation_state) {
    MockProductState product_state;
    product_state.set_version(current_version_->Clone());
    product_state.set_multi_install(multi_install);
    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(
            multi_install ? BrowserDistribution::CHROME_BINARIES :
                BrowserDistribution::CHROME_FRAME);
    FilePath install_path =
        installer::GetChromeInstallPath(system_level, dist);
    product_state.SetUninstallProgram(
        install_path.Append(installer::kSetupExe));
    product_state.AddUninstallSwitch(installer::switches::kUninstall);
    product_state.AddUninstallSwitch(installer::switches::kChromeFrame);
    if (system_level)
      product_state.AddUninstallSwitch(installer::switches::kSystemLevel);
    if (multi_install) {
      product_state.AddUninstallSwitch(installer::switches::kMultiInstall);
      if (ready_mode) {
        product_state.AddUninstallSwitch(
            installer::switches::kChromeFrameReadyMode);
      }
    }

    installation_state->SetProductState(system_level,
                                        BrowserDistribution::CHROME_FRAME,
                                        product_state);
  }

  MockInstallationState* BuildChromeInstallationState(bool system_level,
                                                      bool multi_install) {
    scoped_ptr<MockInstallationState> installation_state(
        new MockInstallationState());
    AddChromeToInstallationState(system_level, multi_install, false,
                                 installation_state.get());
    return installation_state.release();
  }

  static MockInstallerState* BuildBasicInstallerState(
      bool system_install,
      bool multi_install,
      const InstallationState& machine_state,
      InstallerState::Operation operation) {
    scoped_ptr<MockInstallerState> installer_state(new MockInstallerState());

    InstallerState::Level level = system_install ?
        InstallerState::SYSTEM_LEVEL : InstallerState::USER_LEVEL;
    installer_state->set_level(level);
    installer_state->set_operation(operation);
    // Hope this next one isn't checked for now.
    installer_state->set_state_key(L"PROBABLY_INVALID_REG_PATH");
    installer_state->set_package_type(multi_install ?
                                          InstallerState::MULTI_PACKAGE :
                                          InstallerState::SINGLE_PACKAGE);
    return installer_state.release();
  }

  static void AddChromeToInstallerState(
      const InstallationState& machine_state,
      MockInstallerState* installer_state) {
    // Fresh install or upgrade?
    const ProductState* chrome =
        machine_state.GetProductState(installer_state->system_install(),
                                      BrowserDistribution::CHROME_BROWSER);
    if (chrome != NULL) {
      installer_state->AddProductFromState(BrowserDistribution::CHROME_BROWSER,
                                           *chrome);
    } else {
      BrowserDistribution* dist =
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BROWSER);
      scoped_ptr<Product> product(new Product(dist));
      if (installer_state->is_multi_install())
        product->SetOption(installer::kOptionMultiInstall, true);
      installer_state->AddProduct(&product);
    }
  }

  static void AddChromeFrameToInstallerState(
      const InstallationState& machine_state,
      bool ready_mode,
      MockInstallerState* installer_state) {
    // Fresh install or upgrade?
    const ProductState* cf =
        machine_state.GetProductState(installer_state->system_install(),
                                      BrowserDistribution::CHROME_FRAME);
    if (cf != NULL) {
      installer_state->AddProductFromState(BrowserDistribution::CHROME_FRAME,
                                           *cf);
    } else {
      BrowserDistribution* dist =
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_FRAME);
      scoped_ptr<Product> product(new Product(dist));
      if (installer_state->is_multi_install()) {
        product->SetOption(installer::kOptionMultiInstall, true);
        if (ready_mode)
          product->SetOption(installer::kOptionReadyMode, true);
      }
      installer_state->AddProduct(&product);
    }
  }

  static MockInstallerState* BuildChromeInstallerState(
      bool system_install,
      bool multi_install,
      const InstallationState& machine_state,
      InstallerState::Operation operation) {
    scoped_ptr<MockInstallerState> installer_state(
        BuildBasicInstallerState(system_install, multi_install, machine_state,
                                 operation));
    AddChromeToInstallerState(machine_state, installer_state.get());
    return installer_state.release();
  }

  static MockInstallerState* BuildChromeFrameInstallerState(
      bool system_install,
      bool multi_install,
      bool ready_mode,
      const InstallationState& machine_state,
      InstallerState::Operation operation) {
    scoped_ptr<MockInstallerState> installer_state(
        BuildBasicInstallerState(system_install, multi_install, machine_state,
                                 operation));
    AddChromeFrameToInstallerState(machine_state, ready_mode,
                                   installer_state.get());
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
  const bool system_level = true;
  const bool multi_install = false;
  MockWorkItemList work_item_list;

  scoped_ptr<InstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  scoped_ptr<InstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::SINGLE_INSTALL_OR_UPDATE));

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

namespace {

const wchar_t elevation_key[] =
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\ElevationPolicy\\"
    L"{E0A900DF-9611-4446-86BD-4B1D47E7DB2A}";
const wchar_t dragdrop_key[] =
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\DragDrop\\"
    L"{E0A900DF-9611-4446-86BD-4B1D47E7DB2A}";

}  // namespace

TEST_F(InstallWorkerTest, ElevationPolicyWorkItems) {
  const bool system_level = true;
  const HKEY root = HKEY_LOCAL_MACHINE;
  const bool multi_install = true;
  MockWorkItemList work_item_list;

  scoped_ptr<MockInstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::MULTI_INSTALL));

  EXPECT_CALL(work_item_list, AddCreateRegKeyWorkItem(root,
      StrEq(elevation_key))).Times(1);

  EXPECT_CALL(work_item_list, AddCreateRegKeyWorkItem(root,
      StrEq(dragdrop_key))).Times(1);

  EXPECT_CALL(work_item_list, AddSetRegDwordValueWorkItem(root,
      StrEq(elevation_key), StrEq(L"Policy"), 3, _)).Times(1);

  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(root,
      StrEq(elevation_key), StrEq(L"AppName"),
      StrEq(installer::kChromeLauncherExe), _)).Times(1);

  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(root,
      StrEq(elevation_key), StrEq(L"AppPath"), _, _)).Times(1);

  EXPECT_CALL(work_item_list, AddSetRegDwordValueWorkItem(root,
      StrEq(dragdrop_key), StrEq(L"Policy"), 3, _)).Times(1);

  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(root,
      StrEq(dragdrop_key), StrEq(L"AppName"),
      StrEq(chrome::kBrowserProcessExecutableName), _)).Times(1);

  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(root,
      StrEq(dragdrop_key), StrEq(L"AppPath"), _, _)).Times(1);

  AddElevationPolicyWorkItems(*installation_state.get(),
                              *installer_state.get(),
                              *new_version_.get(),
                              &work_item_list);
}

TEST_F(InstallWorkerTest, ElevationPolicyUninstall) {
  const bool system_level = true;
  const HKEY root = HKEY_LOCAL_MACHINE;
  const bool multi_install = true;
  MockWorkItemList work_item_list;

  scoped_ptr<MockInstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::UNINSTALL));

  EXPECT_CALL(work_item_list, AddDeleteRegKeyWorkItem(root,
      StrEq(elevation_key))).Times(1);
  EXPECT_CALL(work_item_list, AddDeleteRegKeyWorkItem(root,
      StrEq(dragdrop_key))).Times(1);

  AddElevationPolicyWorkItems(*installation_state.get(),
                              *installer_state.get(),
                              *new_version_.get(),
                              &work_item_list);
}

TEST_F(InstallWorkerTest, ElevationPolicySingleNoop) {
  const bool system_level = true;
  const HKEY root = HKEY_LOCAL_MACHINE;
  const bool multi_install = false;  // nothing should be done for single.
  MockWorkItemList work_item_list;

  scoped_ptr<MockInstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::UNINSTALL));

  EXPECT_CALL(work_item_list, AddDeleteRegKeyWorkItem(_, _)).Times(0);
  EXPECT_CALL(work_item_list, AddCreateRegKeyWorkItem(_, _)).Times(0);
  EXPECT_CALL(work_item_list, AddSetRegDwordValueWorkItem(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(_, _, _, _, _))
      .Times(0);

  AddElevationPolicyWorkItems(*installation_state.get(),
                              *installer_state.get(),
                              *new_version_.get(),
                              &work_item_list);
}

TEST_F(InstallWorkerTest, ElevationPolicyExistingSingleCFNoop) {
  const bool system_level = true;
  const HKEY root = HKEY_LOCAL_MACHINE;
  const bool multi_install = true;
  MockWorkItemList work_item_list;

  scoped_ptr<MockInstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  MockProductState cf_state;
  cf_state.set_version(current_version_->Clone());
  cf_state.set_multi_install(false);

  installation_state->SetProductState(system_level,
      BrowserDistribution::CHROME_FRAME, cf_state);

  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::MULTI_INSTALL));

  EXPECT_CALL(work_item_list, AddDeleteRegKeyWorkItem(_, _)).Times(0);
  EXPECT_CALL(work_item_list, AddCreateRegKeyWorkItem(_, _)).Times(0);
  EXPECT_CALL(work_item_list, AddSetRegDwordValueWorkItem(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(_, _, _, _, _))
      .Times(0);

  AddElevationPolicyWorkItems(*installation_state.get(),
                              *installer_state.get(),
                              *new_version_.get(),
                              &work_item_list);
}

// Test scenarios under which the quick-enable-cf command should not exist after
// the run.  We're permissive in that we allow the DeleteRegKeyWorkItem even if
// it isn't strictly needed.
class QuickEnableAbsentTest : public InstallWorkerTest {
 public:
  virtual void SetUp() {
    InstallWorkerTest::SetUp();
    root_key_ = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    delete_reg_key_item_.reset(
        WorkItem::CreateDeleteRegKeyWorkItem(root_key_, kRegKeyPath));
    machine_state_.reset(new MockInstallationState());
    EXPECT_CALL(work_item_list_,
                AddDeleteRegKeyWorkItem(Eq(root_key_), StrCaseEq(kRegKeyPath)))
        .Times(AtMost(1))
        .WillRepeatedly(Return(delete_reg_key_item_.get()));
  }
  virtual void TearDown() {
    machine_state_.reset();
    delete_reg_key_item_.reset();
    root_key_ = NULL;
    InstallWorkerTest::TearDown();
  }
 protected:
  static const bool system_level_ = false;
  static const wchar_t kRegKeyPath[];
  HKEY root_key_;
  scoped_ptr<DeleteRegKeyWorkItem> delete_reg_key_item_;
  scoped_ptr<MockInstallationState> machine_state_;
  StrictMock<MockWorkItemList> work_item_list_;
};

const wchar_t QuickEnableAbsentTest::kRegKeyPath[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}\\Commands\\quick-enable-cf";

TEST_F(QuickEnableAbsentTest, CleanInstallSingleChrome) {
  // Install single Chrome on a clean system.
  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level_, false, *machine_state_,
                                InstallerState::SINGLE_INSTALL_OR_UPDATE));
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnableAbsentTest, CleanInstallSingleChromeFrame) {
  // Install single Chrome Frame on a clean system.
  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeFrameInstallerState(system_level_, false, false,
                                     *machine_state_,
                                     InstallerState::SINGLE_INSTALL_OR_UPDATE));
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnableAbsentTest, CleanInstallMultiChromeFrame) {
  // Install multi Chrome Frame on a clean system.
  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeFrameInstallerState(system_level_, true, false,
                                     *machine_state_,
                                     InstallerState::MULTI_INSTALL));
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnableAbsentTest, CleanInstallMultiChromeChromeFrame) {
  // Install multi Chrome and Chrome Frame on a clean system.
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                               InstallerState::MULTI_INSTALL));
  AddChromeToInstallerState(*machine_state_, installer_state.get());
  AddChromeFrameToInstallerState(*machine_state_, false,
                                 installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnableAbsentTest, UninstallMultiChromeLeaveMultiChromeFrame) {
  // Uninstall multi Chrome on a machine with multi Chrome Frame.
  AddChromeToInstallationState(system_level_, true, false,
                               machine_state_.get());
  AddChromeFrameToInstallationState(system_level_, true, false,
                                    machine_state_.get());
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                               InstallerState::UNINSTALL));
  AddChromeToInstallerState(*machine_state_, installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnableAbsentTest, UninstallMultiChromeLeaveSingleChromeFrame) {
  // Uninstall multi Chrome on a machine with single Chrome Frame.
  AddChromeToInstallationState(system_level_, true, false,
                               machine_state_.get());
  AddChromeFrameToInstallationState(system_level_, false, false,
                                    machine_state_.get());
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                               InstallerState::UNINSTALL));
  AddChromeToInstallerState(*machine_state_, installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnableAbsentTest, AcceptReadyMode) {
  // Accept ready-mode.
  AddChromeToInstallationState(system_level_, true, true,
                               machine_state_.get());
  AddChromeFrameToInstallationState(system_level_, true, true,
                                    machine_state_.get());
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                               InstallerState::UNINSTALL));
  AddChromeToInstallerState(*machine_state_, installer_state.get());
  AddChromeFrameToInstallerState(*machine_state_, false, installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

// Test scenarios under which the quick-enable-cf command should exist after the
// run.
class QuickEnablePresentTest : public InstallWorkerTest {
 public:
  virtual void SetUp() {
    InstallWorkerTest::SetUp();
    root_key_ = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    create_reg_key_work_item_.reset(
        WorkItem::CreateCreateRegKeyWorkItem(root_key_, kRegKeyPath));
    set_reg_value_work_item_.reset(
        WorkItem::CreateSetRegValueWorkItem(root_key_, kRegKeyPath, L"", L"",
                                            false));
    machine_state_.reset(new MockInstallationState());
    EXPECT_CALL(work_item_list_,
                AddCreateRegKeyWorkItem(Eq(root_key_), StrCaseEq(kRegKeyPath)))
        .Times(1)
        .WillOnce(Return(create_reg_key_work_item_.get()));
    EXPECT_CALL(work_item_list_,
                AddSetRegStringValueWorkItem(Eq(root_key_),
                                             StrCaseEq(kRegKeyPath),
                                             StrEq(L"CommandLine"), _,
                                             Eq(true)))
        .Times(1)
        .WillOnce(Return(set_reg_value_work_item_.get()));
    EXPECT_CALL(work_item_list_,
                AddSetRegDwordValueWorkItem(Eq(root_key_),
                                            StrCaseEq(kRegKeyPath), _,
                                            Eq(static_cast<DWORD>(1)),
                                            Eq(true)))
        .Times(2)
        .WillRepeatedly(Return(set_reg_value_work_item_.get()));
  }
  virtual void TearDown() {
    machine_state_.reset();
    set_reg_value_work_item_.reset();
    create_reg_key_work_item_.reset();
    root_key_ = NULL;
    InstallWorkerTest::TearDown();
  }
 protected:
  static const bool system_level_ = false;
  static const wchar_t kRegKeyPath[];
  HKEY root_key_;
  scoped_ptr<CreateRegKeyWorkItem> create_reg_key_work_item_;
  scoped_ptr<SetRegValueWorkItem> set_reg_value_work_item_;
  scoped_ptr<MockInstallationState> machine_state_;
  StrictMock<MockWorkItemList> work_item_list_;
};

const wchar_t QuickEnablePresentTest::kRegKeyPath[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}\\Commands\\quick-enable-cf";

TEST_F(QuickEnablePresentTest, CleanInstallMultiChrome) {
  // Install multi Chrome on a clean system.
  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level_, true, *machine_state_,
                                InstallerState::MULTI_INSTALL));
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnablePresentTest, CleanInstallMultiChromeReadyMode) {
  // Install multi Chrome with Chrome Frame ready-mode on a clean system.
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                               InstallerState::MULTI_INSTALL));
  AddChromeToInstallerState(*machine_state_, installer_state.get());
  AddChromeFrameToInstallerState(*machine_state_, true,
                                 installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnablePresentTest, UninstallSingleChromeFrame) {
  // Uninstall single Chrome Frame on a machine with multi Chrome.
  AddChromeToInstallationState(system_level_, true, false,
                               machine_state_.get());
  AddChromeFrameToInstallationState(system_level_, false, false,
                                    machine_state_.get());
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, false, *machine_state_,
                               InstallerState::UNINSTALL));
  AddChromeFrameToInstallerState(*machine_state_, false, installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}

TEST_F(QuickEnablePresentTest, UninstallMultiChromeFrame) {
  // Uninstall multi Chrome Frame on a machine with multi Chrome.
  AddChromeToInstallationState(system_level_, true, false,
                               machine_state_.get());
  AddChromeFrameToInstallationState(system_level_, true, false,
                                    machine_state_.get());
  scoped_ptr<MockInstallerState> installer_state(
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                               InstallerState::UNINSTALL));
  AddChromeFrameToInstallerState(*machine_state_, false, installer_state.get());
  AddQuickEnableWorkItems(*installer_state, *machine_state_, &setup_path_,
                          new_version_.get(), &work_item_list_);
}
