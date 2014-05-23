// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/install_worker.h"

#include "base/win/registry.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/delete_reg_key_work_item.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item_list.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::win::RegKey;
using installer::InstallationState;
using installer::InstallerState;
using installer::Product;
using installer::ProductState;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::HasSubstr;
using ::testing::Eq;
using ::testing::Return;
using ::testing::StrCaseEq;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Values;

// Mock classes to help with testing
//------------------------------------------------------------------------------

class MockWorkItemList : public WorkItemList {
 public:
  MockWorkItemList() {}

  MOCK_METHOD4(AddCopyRegKeyWorkItem, WorkItem* (HKEY,
                                                 const std::wstring&,
                                                 const std::wstring&,
                                                 CopyOverWriteOption));
  MOCK_METHOD5(AddCopyTreeWorkItem, WorkItem*(const std::wstring&,
                                              const std::wstring&,
                                              const std::wstring&,
                                              CopyOverWriteOption,
                                              const std::wstring&));
  MOCK_METHOD1(AddCreateDirWorkItem, WorkItem* (const base::FilePath&));
  MOCK_METHOD2(AddCreateRegKeyWorkItem, WorkItem* (HKEY, const std::wstring&));
  MOCK_METHOD2(AddDeleteRegKeyWorkItem, WorkItem* (HKEY, const std::wstring&));
  MOCK_METHOD3(AddDeleteRegValueWorkItem, WorkItem* (HKEY,
                                                     const std::wstring&,
                                                     const std::wstring&));
  MOCK_METHOD2(AddDeleteTreeWorkItem, WorkItem* (
      const base::FilePath&,
      const std::vector<base::FilePath>&));
  MOCK_METHOD1(AddDeleteTreeWorkItem, WorkItem* (const base::FilePath&));
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
  void set_brand(const std::wstring& brand) { brand_ = brand; }
  void set_eula_accepted(DWORD eula_accepted) {
    has_eula_accepted_ = true;
    eula_accepted_ = eula_accepted;
  }
  void clear_eula_accepted() { has_eula_accepted_ = false; }
  void set_usagestats(DWORD usagestats) {
    has_usagestats_ = true;
    usagestats_ = usagestats;
  }
  void clear_usagestats() { has_usagestats_ = false; }
  void set_oem_install(const std::wstring& oem_install) {
    has_oem_install_ = true;
    oem_install_ = oem_install;
  }
  void clear_oem_install() { has_oem_install_ = false; }
  void SetUninstallProgram(const base::FilePath& setup_exe) {
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

  void set_state_type(BrowserDistribution::Type state_type) {
    state_type_ = state_type;
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
    current_version_.reset(new Version("1.0.0.0"));
    new_version_.reset(new Version("42.0.0.0"));

    // Don't bother ensuring that these paths exist. Since we're just
    // building the work item lists and not running them, they shouldn't
    // actually be touched.
    archive_path_ =
        base::FilePath(L"C:\\UnlikelyPath\\Temp\\chrome_123\\chrome.7z");
    // TODO(robertshield): Take this from the BrowserDistribution once that
    // no longer depends on MasterPreferences.
    installation_path_ =
        base::FilePath(L"C:\\Program Files\\Google\\Chrome\\");
    src_path_ = base::FilePath(
        L"C:\\UnlikelyPath\\Temp\\chrome_123\\source\\Chrome-bin");
    setup_path_ = base::FilePath(
        L"C:\\UnlikelyPath\\Temp\\CR_123.tmp\\setup.exe");
    temp_dir_ = base::FilePath(L"C:\\UnlikelyPath\\Temp\\chrome_123");
  }

  virtual void TearDown() {
  }

  void MaybeAddBinariesToInstallationState(
      bool system_level,
      MockInstallationState* installation_state) {
    if (installation_state->GetProductState(
            system_level, BrowserDistribution::CHROME_BINARIES) == NULL) {
      MockProductState product_state;
      product_state.set_version(new Version(*current_version_));
      product_state.set_brand(L"TEST");
      product_state.set_multi_install(true);
      BrowserDistribution* dist =
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BINARIES);
      base::FilePath install_path =
          installer::GetChromeInstallPath(system_level, dist);
      product_state.SetUninstallProgram(
          install_path.AppendASCII(current_version_->GetString())
          .Append(installer::kInstallerDir)
          .Append(installer::kSetupExe));
      product_state.AddUninstallSwitch(installer::switches::kUninstall);
      product_state.AddUninstallSwitch(installer::switches::kMultiInstall);
      if (system_level)
        product_state.AddUninstallSwitch(installer::switches::kSystemLevel);
      installation_state->SetProductState(system_level,
                                          BrowserDistribution::CHROME_BINARIES,
                                          product_state);
    }
  }

  void AddChromeToInstallationState(
      bool system_level,
      bool multi_install,
      MockInstallationState* installation_state) {
    if (multi_install)
      MaybeAddBinariesToInstallationState(system_level, installation_state);
    MockProductState product_state;
    product_state.set_version(new Version(*current_version_));
    product_state.set_multi_install(multi_install);
    product_state.set_brand(L"TEST");
    product_state.set_eula_accepted(1);
    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_BROWSER);
    base::FilePath install_path =
        installer::GetChromeInstallPath(system_level, dist);
    product_state.SetUninstallProgram(
      install_path.AppendASCII(current_version_->GetString())
          .Append(installer::kInstallerDir)
          .Append(installer::kSetupExe));
    product_state.AddUninstallSwitch(installer::switches::kUninstall);
    if (system_level)
      product_state.AddUninstallSwitch(installer::switches::kSystemLevel);
    if (multi_install) {
      product_state.AddUninstallSwitch(installer::switches::kMultiInstall);
      product_state.AddUninstallSwitch(installer::switches::kChrome);
    }

    installation_state->SetProductState(system_level,
                                        BrowserDistribution::CHROME_BROWSER,
                                        product_state);
  }

  void AddChromeFrameToInstallationState(
      bool system_level,
      bool multi_install,
      MockInstallationState* installation_state) {
    if (multi_install)
      MaybeAddBinariesToInstallationState(system_level, installation_state);
    MockProductState product_state;
    product_state.set_version(new Version(*current_version_));
    product_state.set_multi_install(multi_install);
    BrowserDistribution* dist =
        BrowserDistribution::GetSpecificDistribution(
            multi_install ? BrowserDistribution::CHROME_BINARIES :
                BrowserDistribution::CHROME_FRAME);
    base::FilePath install_path =
        installer::GetChromeInstallPath(system_level, dist);
    product_state.SetUninstallProgram(
      install_path.AppendASCII(current_version_->GetString())
          .Append(installer::kInstallerDir)
          .Append(installer::kSetupExe));
    product_state.AddUninstallSwitch(installer::switches::kUninstall);
    product_state.AddUninstallSwitch(installer::switches::kChromeFrame);
    if (system_level)
      product_state.AddUninstallSwitch(installer::switches::kSystemLevel);
    if (multi_install)
      product_state.AddUninstallSwitch(installer::switches::kMultiInstall);

    installation_state->SetProductState(system_level,
                                        BrowserDistribution::CHROME_FRAME,
                                        product_state);
  }

  MockInstallationState* BuildChromeInstallationState(bool system_level,
                                                      bool multi_install) {
    scoped_ptr<MockInstallationState> installation_state(
        new MockInstallationState());
    AddChromeToInstallationState(system_level, multi_install,
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
    installer_state->set_state_type(BrowserDistribution::CHROME_BROWSER);
    installer_state->set_package_type(multi_install ?
                                          InstallerState::MULTI_PACKAGE :
                                          InstallerState::SINGLE_PACKAGE);
    return installer_state.release();
  }

  static void AddChromeBinariesToInstallerState(
      const InstallationState& machine_state,
      MockInstallerState* installer_state) {
    if (!installer_state->is_multi_install()) {
      NOTREACHED();
      return;
    }
    if (installer_state->FindProduct(BrowserDistribution::CHROME_BINARIES))
      return;

    // Fresh install or upgrade?
    const ProductState* chrome_binaries =
        machine_state.GetProductState(installer_state->system_install(),
                                      BrowserDistribution::CHROME_BINARIES);
    if (chrome_binaries != NULL) {
      installer_state->AddProductFromState(BrowserDistribution::CHROME_BINARIES,
                                           *chrome_binaries);
    } else {
      BrowserDistribution* dist =
          BrowserDistribution::GetSpecificDistribution(
              BrowserDistribution::CHROME_BINARIES);
      scoped_ptr<Product> product(new Product(dist));
      product->SetOption(installer::kOptionMultiInstall, true);
      installer_state->AddProduct(&product);
    }
  }

  static void AddChromeToInstallerState(
      const InstallationState& machine_state,
      MockInstallerState* installer_state) {
    // Fresh install or upgrade?
    const ProductState* chrome =
        machine_state.GetProductState(installer_state->system_install(),
                                      BrowserDistribution::CHROME_BROWSER);
    if (chrome != NULL &&
        chrome->is_multi_install() == installer_state->is_multi_install()) {
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
      if (installer_state->is_multi_install())
        product->SetOption(installer::kOptionMultiInstall, true);
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
    if (multi_install) {
      // We don't want to include Chrome Binaries for uninstall if the machine
      // has other products. For simplicity, we check Chrome Frame only.
      bool machine_has_other_products =
          machine_state.GetProductState(system_install,
              BrowserDistribution::CHROME_FRAME) != NULL;
      if (operation != InstallerState::UNINSTALL || !machine_has_other_products)
        AddChromeBinariesToInstallerState(machine_state, installer_state.get());
    }
    AddChromeToInstallerState(machine_state, installer_state.get());
    return installer_state.release();
  }

  static MockInstallerState* BuildChromeFrameInstallerState(
      bool system_install,
      bool multi_install,
      const InstallationState& machine_state,
      InstallerState::Operation operation) {
    // This method only works for installation/upgrade.
    DCHECK(operation != InstallerState::UNINSTALL);
    scoped_ptr<MockInstallerState> installer_state(
        BuildBasicInstallerState(system_install, multi_install, machine_state,
                                 operation));
    if (multi_install)
      AddChromeBinariesToInstallerState(machine_state, installer_state.get());
    AddChromeFrameToInstallerState(machine_state, installer_state.get());
    return installer_state.release();
  }

 protected:
  scoped_ptr<Version> current_version_;
  scoped_ptr<Version> new_version_;
  base::FilePath archive_path_;
  base::FilePath installation_path_;
  base::FilePath setup_path_;
  base::FilePath src_path_;
  base::FilePath temp_dir_;
};

// Tests
//------------------------------------------------------------------------------

TEST_F(InstallWorkerTest, TestInstallChromeSingleSystem) {
  const bool system_level = true;
  const bool multi_install = false;
  MockWorkItemList work_item_list;

  const HKEY kRegRoot = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  static const wchar_t kRegKeyPath[] = L"Software\\Chromium\\test";
  scoped_ptr<CreateRegKeyWorkItem> create_reg_key_work_item(
      WorkItem::CreateCreateRegKeyWorkItem(
          kRegRoot, kRegKeyPath, WorkItem::kWow64Default));
  scoped_ptr<SetRegValueWorkItem> set_reg_value_work_item(
      WorkItem::CreateSetRegValueWorkItem(
          kRegRoot, kRegKeyPath, WorkItem::kWow64Default, L"", L"", false));

  scoped_ptr<InstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  scoped_ptr<InstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::SINGLE_INSTALL_OR_UPDATE));

  // Set up some expectations.
  // TODO(robertshield): Set up some real expectations.
  EXPECT_CALL(work_item_list, AddCopyTreeWorkItem(_, _, _, _, _))
      .Times(AtLeast(1));
  EXPECT_CALL(work_item_list, AddCreateRegKeyWorkItem(_, _))
      .WillRepeatedly(Return(create_reg_key_work_item.get()));
  EXPECT_CALL(work_item_list, AddSetRegStringValueWorkItem(_, _, _, _, _))
      .WillRepeatedly(Return(set_reg_value_work_item.get()));

  AddInstallWorkItems(*installation_state.get(),
                      *installer_state.get(),
                      setup_path_,
                      archive_path_,
                      src_path_,
                      temp_dir_,
                      current_version_.get(),
                      *new_version_.get(),
                      &work_item_list);
}

namespace {

const wchar_t elevation_key[] =
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\ElevationPolicy\\"
    L"{E0A900DF-9611-4446-86BD-4B1D47E7DB2A}";
const wchar_t old_elevation_key[] =
    L"SOFTWARE\\Microsoft\\Internet Explorer\\Low Rights\\ElevationPolicy\\"
    L"{6C288DD7-76FB-4721-B628-56FAC252E199}";

}  // namespace

// A test class for worker functions that manipulate the old IE low rights
// policies.
// Parameters:
// bool : system_level_
// bool : multi_install_
class OldIELowRightsTests : public InstallWorkerTest,
  public ::testing::WithParamInterface<std::tr1::tuple<bool, bool> > {
 protected:
  virtual void SetUp() OVERRIDE {
    InstallWorkerTest::SetUp();

    const ParamType& param = GetParam();
    system_level_ = std::tr1::get<0>(param);
    multi_install_ = std::tr1::get<1>(param);
    root_key_ = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

    installation_state_.reset(new MockInstallationState());
    AddChromeFrameToInstallationState(system_level_, multi_install_,
                                      installation_state_.get());
    installer_state_.reset(BuildBasicInstallerState(
        system_level_, multi_install_, *installation_state_,
        multi_install_ ? InstallerState::MULTI_UPDATE :
            InstallerState::SINGLE_INSTALL_OR_UPDATE));
    if (multi_install_)
      AddChromeBinariesToInstallerState(*installation_state_,
                                        installer_state_.get());
    AddChromeFrameToInstallerState(*installation_state_,
                                   installer_state_.get());
  }

  scoped_ptr<MockInstallationState> installation_state_;
  scoped_ptr<MockInstallerState> installer_state_;
  bool system_level_;
  bool multi_install_;
  HKEY root_key_;
};

TEST_P(OldIELowRightsTests, AddDeleteOldIELowRightsPolicyWorkItems) {
  StrictMock<MockWorkItemList> work_item_list;

  EXPECT_CALL(work_item_list,
              AddDeleteRegKeyWorkItem(root_key_, StrEq(old_elevation_key)))
      .Times(1);

  AddDeleteOldIELowRightsPolicyWorkItems(*installer_state_.get(),
                                         &work_item_list);
}

INSTANTIATE_TEST_CASE_P(Variations, OldIELowRightsTests,
                        Combine(Bool(), Bool()));

TEST_F(InstallWorkerTest, GoogleUpdateWorkItemsTest) {
  const bool system_level = true;
  const bool multi_install = true;
  MockWorkItemList work_item_list;

  scoped_ptr<MockInstallationState> installation_state(
      BuildChromeInstallationState(system_level, false));

  MockProductState cf_state;
  cf_state.set_version(new Version(*current_version_));
  cf_state.set_multi_install(false);

  installation_state->SetProductState(system_level,
      BrowserDistribution::CHROME_FRAME, cf_state);

  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::MULTI_INSTALL));

  // Expect the multi Client State key to be created.
  BrowserDistribution* multi_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  std::wstring multi_app_guid(multi_dist->GetAppGuid());
  std::wstring multi_client_state_suffix(L"ClientState\\" + multi_app_guid);
  EXPECT_CALL(work_item_list,
              AddCreateRegKeyWorkItem(_, HasSubstr(multi_client_state_suffix)))
      .Times(testing::AnyNumber());

  // Expect ClientStateMedium to be created for system-level installs.
  EXPECT_CALL(work_item_list,
              AddCreateRegKeyWorkItem(_, HasSubstr(L"ClientStateMedium\\" +
                                                   multi_app_guid)))
      .Times(system_level ? 1 : 0);

  // Expect to see a set value for the "TEST" brand code in the multi Client
  // State key.
  EXPECT_CALL(work_item_list,
              AddSetRegStringValueWorkItem(_,
                                           HasSubstr(multi_client_state_suffix),
                                           StrEq(google_update::kRegBrandField),
                                           StrEq(L"TEST"),
                                           _)).Times(1);

  // There may also be some calls to set 'ap' values.
  EXPECT_CALL(work_item_list,
              AddSetRegStringValueWorkItem(_, _,
                                           StrEq(google_update::kRegApField),
                                           _, _)).Times(testing::AnyNumber());

  // Expect "oeminstall" to be cleared.
  EXPECT_CALL(work_item_list,
              AddDeleteRegValueWorkItem(
                  _,
                  HasSubstr(multi_client_state_suffix),
                  StrEq(google_update::kRegOemInstallField))).Times(1);

  // Expect "eulaaccepted" to set.
  EXPECT_CALL(work_item_list,
              AddSetRegDwordValueWorkItem(
                  _,
                  HasSubstr(multi_client_state_suffix),
                  StrEq(google_update::kRegEULAAceptedField),
                  Eq(static_cast<DWORD>(1)),
                  _)).Times(1);

  AddGoogleUpdateWorkItems(*installation_state.get(),
                           *installer_state.get(),
                           &work_item_list);
}

// Test that usagestats values are migrated properly.
TEST_F(InstallWorkerTest, AddUsageStatsWorkItems) {
  const bool system_level = true;
  const bool multi_install = true;
  MockWorkItemList work_item_list;

  scoped_ptr<MockInstallationState> installation_state(
      BuildChromeInstallationState(system_level, multi_install));

  MockProductState chrome_state;
  chrome_state.set_version(new Version(*current_version_));
  chrome_state.set_multi_install(false);
  chrome_state.set_usagestats(1);

  installation_state->SetProductState(system_level,
      BrowserDistribution::CHROME_BROWSER, chrome_state);

  scoped_ptr<MockInstallerState> installer_state(
      BuildChromeInstallerState(system_level, multi_install,
                                *installation_state,
                                InstallerState::MULTI_INSTALL));

  // Expect the multi Client State key to be created.
  BrowserDistribution* multi_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BINARIES);
  std::wstring multi_app_guid(multi_dist->GetAppGuid());
  EXPECT_CALL(work_item_list,
              AddCreateRegKeyWorkItem(_, HasSubstr(multi_app_guid))).Times(1);

  // Expect to see a set value for the usagestats in the multi Client State key.
  EXPECT_CALL(work_item_list,
              AddSetRegDwordValueWorkItem(
                  _,
                  HasSubstr(multi_app_guid),
                  StrEq(google_update::kRegUsageStatsField),
                  Eq(static_cast<DWORD>(1)),
                  Eq(true))).Times(1);

  // Expect to see some values cleaned up from Chrome's keys.
  BrowserDistribution* chrome_dist =
      BrowserDistribution::GetSpecificDistribution(
          BrowserDistribution::CHROME_BROWSER);
  if (system_level) {
    EXPECT_CALL(work_item_list,
                AddDeleteRegValueWorkItem(
                    _,
                    StrEq(chrome_dist->GetStateMediumKey()),
                    StrEq(google_update::kRegUsageStatsField))).Times(1);
    EXPECT_CALL(work_item_list,
                AddDeleteRegValueWorkItem(
                    Eq(HKEY_CURRENT_USER),
                    StrEq(chrome_dist->GetStateKey()),
                    StrEq(google_update::kRegUsageStatsField))).Times(1);
  }
  EXPECT_CALL(work_item_list,
              AddDeleteRegValueWorkItem(
                  Eq(installer_state->root_key()),
                  StrEq(chrome_dist->GetStateKey()),
                  StrEq(google_update::kRegUsageStatsField))).Times(1);

  AddUsageStatsWorkItems(*installation_state.get(),
                         *installer_state.get(),
                         &work_item_list);
}

// The Quick Enable tests only make sense for the Google Chrome build as it
// interacts with registry values that are specific to Google Update.
#if defined(GOOGLE_CHROME_BUILD)

// Test scenarios under which the quick-enable-cf command should not exist after
// the run.  We're permissive in that we allow the DeleteRegKeyWorkItem even if
// it isn't strictly needed.
class QuickEnableAbsentTest : public InstallWorkerTest {
 public:
  virtual void SetUp() {
    InstallWorkerTest::SetUp();
    root_key_ = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    delete_reg_key_item_.reset(WorkItem::CreateDeleteRegKeyWorkItem(
        root_key_, kRegKeyPath, WorkItem::kWow64Default));
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
      BuildBasicInstallerState(system_level_, true, *machine_state_,
                                InstallerState::MULTI_UPDATE));
  AddQuickEnableChromeFrameWorkItems(*installer_state, &work_item_list_);
}

TEST_F(InstallWorkerTest, WillProductBePresentAfterSetup) {
  BrowserDistribution::Type prod_type_list[] = {
    BrowserDistribution::CHROME_BROWSER,
    BrowserDistribution::CHROME_FRAME,
    // Excluding BrowserDistribution::CHROME_BINARIES, since it is installed
    // along with other products.
  };
  enum {  // Index into prod_type_list[].
    TYPE_BROWSER = 0,
    TYPE_CF,
    NUM_TYPE  // This must appear last.
  };
  DCHECK(arraysize(prod_type_list) == NUM_TYPE);
  InstallerState::Operation op_list[] = {
    InstallerState::UNINSTALL,
    InstallerState::SINGLE_INSTALL_OR_UPDATE
  };

  const bool system_level = false;
  const bool multi_install = true;

  // Loop over machine states: {No product, Chrome, CF, Chrome + CF}.
  for (int i_mach = 0; i_mach < (1 << NUM_TYPE); ++i_mach) {
    // i_mach is the machine state before operation, as bit mask.
    scoped_ptr<MockInstallationState> machine_state(
        new MockInstallationState());
    if ((i_mach & (1 << TYPE_BROWSER)) != 0) {  // Add Chrome.
      AddChromeToInstallationState(system_level, multi_install,
                                   machine_state.get());
    }
    if ((i_mach & (1 << TYPE_CF)) != 0) {  // Add Chrome Frame.
      AddChromeFrameToInstallationState(system_level, multi_install,
                                        machine_state.get());
    }

    // Loop over operations: {uninstall, install/update}.
    for (int i_op = 0; i_op < arraysize(op_list); ++i_op) {

      // Loop over product types to operate on: {TYPE_BROWSER, TYPE_CF}.
      for (int i_type_op = 0; i_type_op < NUM_TYPE; ++i_type_op) {
        scoped_ptr<InstallerState> installer_state;
        if (i_type_op == TYPE_BROWSER) {
          installer_state.reset(BuildChromeInstallerState(
              system_level, multi_install, *machine_state, op_list[i_op]));
        } else if (i_type_op == TYPE_CF) {
          // Skip the CF uninstall case due to limitations in
          // BuildChromeFrameInstallerState().
          if (op_list[i_op] == InstallerState::UNINSTALL)
            continue;

          installer_state.reset(BuildChromeFrameInstallerState(
              system_level, multi_install, *machine_state, op_list[i_op]));
        } else {
          NOTREACHED();
        }

        // Calculate the machine state after operation, as bit mask.
        // If uninstall, remove product with bitwise AND; else add with OR.
        int mach_after = (op_list[i_op] == InstallerState::UNINSTALL) ?
            i_mach & ~(1 << i_type_op) : i_mach | (1 << i_type_op);

        // Verify predicted presence of Chrome Binaries.
        bool bin_res = installer::WillProductBePresentAfterSetup(
            *installer_state,
            *machine_state,
            BrowserDistribution::CHROME_BINARIES);
        // Binaries are expected to be present iff any product is installed.
        bool bin_expect = mach_after != 0;
        EXPECT_EQ(bin_expect, bin_res);

        // Loop over product types to check: {TYPE_BROWSER, TYPE_CF}.
        for (int i_type_check = 0; i_type_check < NUM_TYPE; ++i_type_check) {
          // Verify predicted presence of product.
          bool prod_res = installer::WillProductBePresentAfterSetup(
              *installer_state,
              *machine_state,
              prod_type_list[i_type_check]);
          bool prod_expect = (mach_after & (1 << i_type_check)) != 0;
          EXPECT_EQ(prod_expect, prod_res);
        }
      }
    }
  }
}

#endif  // defined(GOOGLE_CHROME_BUILD)
