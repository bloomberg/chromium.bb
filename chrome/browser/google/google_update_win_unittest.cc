// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_win.h"

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>

#include <queue>

#include "base/base_paths.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "google_update/google_update_idl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"
#include "version.h"

using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::Unused;
using ::testing::Values;
using ::testing::_;

namespace {

class UpdateCheckCallbackReceiver {
 public:
  UpdateCheckCallbackReceiver() {}
  virtual ~UpdateCheckCallbackReceiver() {}
  virtual void OnUpdateCheckCallback(GoogleUpdateUpgradeResult result,
                                     GoogleUpdateErrorCode error_code,
                                     const base::string16& error_message,
                                     const base::string16& version) = 0;
  UpdateCheckCallback GetCallback() {
    return base::Bind(&UpdateCheckCallbackReceiver::UpdateCheckCallback,
                      base::Unretained(this));
  }

 private:
  void UpdateCheckCallback(GoogleUpdateUpgradeResult result,
                           GoogleUpdateErrorCode error_code,
                           const base::string16& error_message,
                           const base::string16& version) {
    OnUpdateCheckCallback(result, error_code, error_message, version);
  }

  DISALLOW_COPY_AND_ASSIGN(UpdateCheckCallbackReceiver);
};

class MockUpdateCheckCallbackReceiver : public UpdateCheckCallbackReceiver {
 public:
  MockUpdateCheckCallbackReceiver() {}
  MOCK_METHOD4(OnUpdateCheckCallback,
               void(GoogleUpdateUpgradeResult,
                    GoogleUpdateErrorCode,
                    const base::string16&,
                    const base::string16&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockUpdateCheckCallbackReceiver);
};

class GoogleUpdateFactory {
 public:
  virtual ~GoogleUpdateFactory() {}
  virtual HRESULT Create(base::win::ScopedComPtr<IGoogleUpdate>* on_demand) = 0;
};

class MockGoogleUpdateFactory : public GoogleUpdateFactory {
 public:
  MockGoogleUpdateFactory() {}
  MOCK_METHOD1(Create, HRESULT(base::win::ScopedComPtr<IGoogleUpdate>*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGoogleUpdateFactory);
};

// A mock IGoogleUpdate on-demand update class that can run an IJobObserver
// through a set of states.
class MockOnDemand : public CComObjectRootEx<CComSingleThreadModel>,
                     public IGoogleUpdate {
 public:
  BEGIN_COM_MAP(MockOnDemand)
    COM_INTERFACE_ENTRY(IGoogleUpdate)
  END_COM_MAP()

  MockOnDemand() : task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             CheckForUpdate,
                             HRESULT(const wchar_t*, IJobObserver*));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Update,
                             HRESULT(const wchar_t*, IJobObserver*));

  void OnCheckRunUpToDateSequence(const base::char16* app_guid) {
    EXPECT_CALL(*this, CheckForUpdate(StrEq(app_guid), _))
        .WillOnce(DoAll(Invoke(this, &MockOnDemand::BeginUpToDateSequence),
                        Return(S_OK)));
  }

  void OnCheckRunUpdateAvailableSequence(const base::char16* app_guid,
                                         const base::string16& new_version) {
    new_version_ = new_version;
    EXPECT_CALL(*this, CheckForUpdate(StrEq(app_guid), _))
        .WillOnce(
            DoAll(Invoke(this, &MockOnDemand::BeginUpdateAvailableSequence),
                  Return(S_OK)));
  }

  void OnUpdateRunInstallUpdateSequence(const base::char16* app_guid,
                                        const base::string16& new_version) {
    new_version_ = new_version;
    EXPECT_CALL(*this, Update(StrEq(app_guid), _))
        .WillOnce(DoAll(Invoke(this, &MockOnDemand::BeginInstallUpdateSequence),
                        Return(S_OK)));
  }

  void OnUpdateRunUpdateErrorSequence(const base::char16* app_guid,
                                      const base::char16* error_text) {
    error_text_ = error_text;
    EXPECT_CALL(*this, Update(StrEq(app_guid), _))
        .WillOnce(DoAll(Invoke(this, &MockOnDemand::BeginUpdateErrorSequence),
                        Return(S_OK)));
  }

 private:
  enum State {
    STATE_CHECKING,
    STATE_COMPLETE_SUCCESS,
    STATE_UPDATE_AVAILABLE,
    STATE_WAITING_TO_DOWNLOAD,
    STATE_DOWNLOADING_25,
    STATE_DOWNLOADING_100,
    STATE_WAITING_TO_INSTALL,
    STATE_INSTALLING,
    STATE_COMPLETE_ERROR,
  };

  void BeginUpToDateSequence(Unused, IJobObserver* job_observer_ptr) {
    job_observer_ = job_observer_ptr;
    states_.push(STATE_CHECKING);
    states_.push(STATE_COMPLETE_SUCCESS);
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockOnDemand::Advance, base::Unretained(this)));
  }

  void BeginUpdateAvailableSequence(Unused, IJobObserver* job_observer_ptr) {
    job_observer_ = job_observer_ptr;
    states_.push(STATE_CHECKING);
    states_.push(STATE_UPDATE_AVAILABLE);
    states_.push(STATE_COMPLETE_SUCCESS);
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockOnDemand::Advance, base::Unretained(this)));
  }

  void BeginInstallUpdateSequence(Unused, IJobObserver* job_observer_ptr) {
    job_observer_ = job_observer_ptr;
    states_.push(STATE_CHECKING);
    states_.push(STATE_UPDATE_AVAILABLE);
    states_.push(STATE_WAITING_TO_DOWNLOAD);
    states_.push(STATE_DOWNLOADING_25);
    states_.push(STATE_DOWNLOADING_100);
    states_.push(STATE_WAITING_TO_INSTALL);
    states_.push(STATE_INSTALLING);
    states_.push(STATE_COMPLETE_SUCCESS);
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockOnDemand::Advance, base::Unretained(this)));
  }

  void BeginUpdateErrorSequence(Unused, IJobObserver* job_observer_ptr) {
    job_observer_ = job_observer_ptr;
    states_.push(STATE_CHECKING);
    states_.push(STATE_UPDATE_AVAILABLE);
    states_.push(STATE_WAITING_TO_DOWNLOAD);
    states_.push(STATE_DOWNLOADING_25);
    states_.push(STATE_DOWNLOADING_100);
    states_.push(STATE_WAITING_TO_INSTALL);
    states_.push(STATE_INSTALLING);
    states_.push(STATE_COMPLETE_ERROR);
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&MockOnDemand::Advance, base::Unretained(this)));
  }

  // Advance to the next state. If this state is non-terminal, a task is posted
  // to advance to the next state a bit later.
  void Advance() {
    ASSERT_FALSE(states_.empty());
    switch (states_.front()) {
      case STATE_CHECKING:
        EXPECT_EQ(S_OK, job_observer_->OnCheckingForUpdate());
        break;
      case STATE_COMPLETE_SUCCESS:
        EXPECT_EQ(S_OK,
                  job_observer_->OnComplete(COMPLETION_CODE_SUCCESS, nullptr));
        break;
      case STATE_UPDATE_AVAILABLE:
        EXPECT_EQ(S_OK, job_observer_->OnUpdateAvailable(new_version_.c_str()));
        break;
      case STATE_WAITING_TO_DOWNLOAD:
        EXPECT_EQ(S_OK, job_observer_->OnWaitingToDownload());
        break;
      case STATE_DOWNLOADING_25:
        EXPECT_EQ(S_OK, job_observer_->OnDownloading(47, 25));
        break;
      case STATE_DOWNLOADING_100:
        EXPECT_EQ(S_OK, job_observer_->OnDownloading(42, 100));
        break;
      case STATE_WAITING_TO_INSTALL:
        EXPECT_EQ(S_OK, job_observer_->OnWaitingToInstall());
        break;
      case STATE_INSTALLING:
        EXPECT_EQ(S_OK, job_observer_->OnInstalling());
        break;
      case STATE_COMPLETE_ERROR:
        EXPECT_EQ(S_OK, job_observer_->OnComplete(COMPLETION_CODE_ERROR,
                                                  error_text_.c_str()));
        break;
    }
    states_.pop();
    if (states_.empty()) {
      // Drop the reference to the observer when the terminal state is reached.
      job_observer_ = nullptr;
    } else {
      task_runner_->PostTask(FROM_HERE, base::Bind(&MockOnDemand::Advance,
                                                   base::Unretained(this)));
    }
  }

  // The task runner on which the state machine runs.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The new version for a successful update check or update.
  base::string16 new_version_;

  // Error text to be supplied for an unsuccessful update check or update.
  base::string16 error_text_;

  // The set of states to be run on an IJobObserver.
  std::queue<State> states_;

  // The IJobObserver given to either CheckForUpdate() or Update() that is being
  // driven through the desired state transitions.
  base::win::ScopedComPtr<IJobObserver> job_observer_;

  DISALLOW_COPY_AND_ASSIGN(MockOnDemand);
};

}  // namespace

class GoogleUpdateWinTest : public ::testing::TestWithParam<bool> {
 public:
  static void SetUpTestCase() { ui::win::CreateATLModuleIfNeeded(); }

 protected:
  GoogleUpdateWinTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_handle_(task_runner_),
        system_level_(GetParam()),
        mock_on_demand_(nullptr) {}

  void SetUp() override {
    ::testing::TestWithParam<bool>::SetUp();

    // Override FILE_EXE so that it looks like the test is running from the
    // standard install location for this mode (system-level or user-level).
    base::FilePath file_exe;
    ASSERT_TRUE(PathService::Get(base::FILE_EXE, &file_exe));
    base::FilePath install_dir(installer::GetChromeInstallPath(
        system_level_, BrowserDistribution::GetDistribution()));
    file_exe_override_.reset(new base::ScopedPathOverride(
        base::FILE_EXE, install_dir.Append(file_exe.BaseName()),
        true /* is_absolute */, false /* create */));

    // Override these paths so that they can be found after the registry
    // override manager is in place.
    base::FilePath temp;
    PathService::Get(base::DIR_PROGRAM_FILES, &temp);
    program_files_override_.reset(
        new base::ScopedPathOverride(base::DIR_PROGRAM_FILES, temp));
    PathService::Get(base::DIR_PROGRAM_FILESX86, &temp);
    program_files_x86_override_.reset(
        new base::ScopedPathOverride(base::DIR_PROGRAM_FILESX86, temp));
    PathService::Get(base::DIR_LOCAL_APP_DATA, &temp);
    local_app_data_override_.reset(
        new base::ScopedPathOverride(base::DIR_LOCAL_APP_DATA, temp));

    // Override the registry so that tests can freely push state to it.
    registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER);
    registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE);

    // Chrome is installed as multi-install.
    const HKEY root = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    base::win::RegKey key(root, kClients, KEY_WRITE | KEY_WOW64_32KEY);
    ASSERT_EQ(ERROR_SUCCESS,
              key.CreateKey(kChromeGuid, KEY_WRITE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(
                  L"pv", base::ASCIIToUTF16(CHROME_VERSION_STRING).c_str()));
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(root, kClientState, KEY_WRITE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.CreateKey(kChromeGuid, KEY_WRITE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"UninstallArguments",
                             L"--uninstall --multi-install --chrome"));

    // Provide an IGoogleUpdate on-demand update class factory so that this test
    // can provide a mocked-out instance.
    SetGoogleUpdateFactoryForTesting(
        base::Bind(&GoogleUpdateFactory::Create,
                   base::Unretained(&mock_google_update_factory_)));
    // Configure the factory to return a generic failure by default.
    ON_CALL(mock_google_update_factory_, Create(_))
        .WillByDefault(Return(E_FAIL));

    // Create a mock IGoogleUpdate on-demand update class
    ASSERT_EQ(S_OK, CComObject<MockOnDemand>::CreateInstance(&mock_on_demand_));
    on_demand_holder_ = mock_on_demand_;
    // Configure the mock to return a generic failure by default.
    ON_CALL(*mock_on_demand_, CheckForUpdate(_, _))
        .WillByDefault(Return(E_FAIL));
    ON_CALL(*mock_on_demand_, Update(_, _)).WillByDefault(Return(E_FAIL));

    // Compute a newer version.
    base::Version current_version(CHROME_VERSION_STRING);
    new_version_ = base::StringPrintf(L"%u.%u.%u.%u",
                                      current_version.components()[0],
                                      current_version.components()[1],
                                      current_version.components()[2] + 1,
                                      current_version.components()[3]);
  }

  void TearDown() override {
    // Remove the test's IGoogleUpdate on-demand update class factory.
    SetGoogleUpdateFactoryForTesting(OnDemandAppsClassFactory());
  }

  // Set the default update policy in the registry.
  void SetDefaultUpdatePolicy(GoogleUpdateSettings::UpdatePolicy policy) const {
    base::win::RegKey policy_key(HKEY_LOCAL_MACHINE, kPoliciesKey,
                                 KEY_SET_VALUE);
    ASSERT_EQ(ERROR_SUCCESS, policy_key.WriteValue(kUpdateDefault, policy));
  }

  // Stuffs |policy| in the registry for the app identified by |app_guid|.
  void SetAppUpdatePolicy(const base::char16* app_guid,
                          GoogleUpdateSettings::UpdatePolicy policy) const {
    base::string16 value_name(L"Update");
    value_name += app_guid;
    base::win::RegKey policy_key(HKEY_LOCAL_MACHINE, kPoliciesKey,
                                 KEY_SET_VALUE);
    ASSERT_EQ(ERROR_SUCCESS, policy_key.WriteValue(value_name.c_str(), policy));
  }

  static const base::char16 kPoliciesKey[];
  static const base::char16 kUpdateDefault[];
  static const base::char16 kClients[];
  static const base::char16 kClientState[];
  static const base::char16 kChromeGuid[];
  static const base::char16 kChromeBinariesGuid[];

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  bool system_level_;
  scoped_ptr<base::ScopedPathOverride> file_exe_override_;
  scoped_ptr<base::ScopedPathOverride> program_files_override_;
  scoped_ptr<base::ScopedPathOverride> program_files_x86_override_;
  scoped_ptr<base::ScopedPathOverride> local_app_data_override_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  MockUpdateCheckCallbackReceiver callback_receiver_;
  MockGoogleUpdateFactory mock_google_update_factory_;
  CComObject<MockOnDemand>* mock_on_demand_;
  base::win::ScopedComPtr<IGoogleUpdate> on_demand_holder_;
  base::string16 new_version_;

  DISALLOW_COPY_AND_ASSIGN(GoogleUpdateWinTest);
};

//  static
const base::char16 GoogleUpdateWinTest::kPoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const base::char16 GoogleUpdateWinTest::kUpdateDefault[] = L"UpdateDefault";
const base::char16 GoogleUpdateWinTest::kClients[] =
    L"Software\\Google\\Update\\Clients";
const base::char16 GoogleUpdateWinTest::kClientState[] =
    L"Software\\Google\\Update\\ClientState";
const base::char16 GoogleUpdateWinTest::kChromeGuid[] =
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";
const base::char16 GoogleUpdateWinTest::kChromeBinariesGuid[] =
    L"{4DC8B4CA-1BDA-483e-B5FA-D3C12E15B62D}";

// Test that an update check fails with the proper error code if Chrome isn't in
// one of the expected install directories.
TEST_P(GoogleUpdateWinTest, InvalidInstallDirectory) {
  // Override FILE_EXE so that it looks like the test is running from a
  // non-standard location.
  base::FilePath file_exe;
  base::FilePath dir_temp;
  ASSERT_TRUE(PathService::Get(base::FILE_EXE, &file_exe));
  ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &dir_temp));
  file_exe_override_.reset();
  file_exe_override_.reset(new base::ScopedPathOverride(
      base::FILE_EXE, dir_temp.Append(file_exe.BaseName()),
      true /* is_absolute */, false /* create */));

  EXPECT_CALL(
      callback_receiver_,
      OnUpdateCheckCallback(UPGRADE_ERROR,
                            CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

#if defined(GOOGLE_CHROME_BUILD)

TEST_P(GoogleUpdateWinTest, AllUpdatesDisabledByPolicy) {
  // Disable updates altogether.
  SetDefaultUpdatePolicy(GoogleUpdateSettings::UPDATES_DISABLED);

  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(UPGRADE_ERROR,
                                    GOOGLE_UPDATE_DISABLED_BY_POLICY, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, MultiUpdatesDisabledByPolicy) {
  // Disable updates altogether.
  SetAppUpdatePolicy(kChromeBinariesGuid,
                     GoogleUpdateSettings::UPDATES_DISABLED);

  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(UPGRADE_ERROR,
                                    GOOGLE_UPDATE_DISABLED_BY_POLICY, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, AllManualUpdatesDisabledByPolicy) {
  // Disable updates altogether.
  SetDefaultUpdatePolicy(GoogleUpdateSettings::AUTO_UPDATES_ONLY);

  EXPECT_CALL(
      callback_receiver_,
      OnUpdateCheckCallback(UPGRADE_ERROR,
                            GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, MultiManualUpdatesDisabledByPolicy) {
  // Disable updates altogether.
  SetAppUpdatePolicy(kChromeBinariesGuid,
                     GoogleUpdateSettings::AUTO_UPDATES_ONLY);

  EXPECT_CALL(
      callback_receiver_,
      OnUpdateCheckCallback(UPGRADE_ERROR,
                            GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, NoGoogleUpdateForCheck) {
  // The factory should be called upon: let it fail.
  EXPECT_CALL(mock_google_update_factory_, Create(_));

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(
                  UPGRADE_ERROR, GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, NoGoogleUpdateForUpgrade) {
  // The factory should be called upon: let it fail.
  EXPECT_CALL(mock_google_update_factory_, Create(_));

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(
                  UPGRADE_ERROR, GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, _, _));
  BeginUpdateCheck(task_runner_, true, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, FailUpdateCheck) {
  // The factory should be called upon: let it return the mock on-demand class.
  EXPECT_CALL(mock_google_update_factory_, Create(_))
      .WillOnce(DoAll(SetArgPointee<0>(mock_on_demand_), Return(S_OK)));
  // The mock on-demand class should be called.
  EXPECT_CALL(*mock_on_demand_, CheckForUpdate(StrEq(kChromeBinariesGuid), _));

  EXPECT_CALL(
      callback_receiver_,
      OnUpdateCheckCallback(UPGRADE_ERROR,
                            GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR, _, _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, UpdateCheckNoUpdate) {
  EXPECT_CALL(mock_google_update_factory_, Create(_))
      .WillOnce(DoAll(SetArgPointee<0>(mock_on_demand_), Return(S_OK)));
  mock_on_demand_->OnCheckRunUpToDateSequence(kChromeBinariesGuid);

  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(UPGRADE_ALREADY_UP_TO_DATE,
                                    GOOGLE_UPDATE_NO_ERROR, IsEmpty(), _));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, UpdateCheckUpdateAvailable) {
  EXPECT_CALL(mock_google_update_factory_, Create(_))
      .WillOnce(DoAll(SetArgPointee<0>(mock_on_demand_), Return(S_OK)));
  mock_on_demand_->OnCheckRunUpdateAvailableSequence(kChromeBinariesGuid,
                                                     new_version_);

  EXPECT_CALL(
      callback_receiver_,
      OnUpdateCheckCallback(UPGRADE_IS_AVAILABLE, GOOGLE_UPDATE_NO_ERROR,
                            IsEmpty(), StrEq(new_version_)));
  BeginUpdateCheck(task_runner_, false, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, UpdateInstalled) {
  EXPECT_CALL(mock_google_update_factory_, Create(_))
      .WillOnce(DoAll(SetArgPointee<0>(mock_on_demand_), Return(S_OK)));
  mock_on_demand_->OnUpdateRunInstallUpdateSequence(kChromeBinariesGuid,
                                                    new_version_);

  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(UPGRADE_SUCCESSFUL, GOOGLE_UPDATE_NO_ERROR,
                                    IsEmpty(), StrEq(new_version_)));
  BeginUpdateCheck(task_runner_, true, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

TEST_P(GoogleUpdateWinTest, UpdateFailed) {
  static const base::char16 kError[] = L"It didn't work.";
  EXPECT_CALL(mock_google_update_factory_, Create(_))
      .WillOnce(DoAll(SetArgPointee<0>(mock_on_demand_), Return(S_OK)));
  mock_on_demand_->OnUpdateRunUpdateErrorSequence(kChromeBinariesGuid, kError);

  EXPECT_CALL(callback_receiver_,
              OnUpdateCheckCallback(UPGRADE_ERROR, GOOGLE_UPDATE_ERROR_UPDATING,
                                    StrEq(kError), IsEmpty()));
  BeginUpdateCheck(task_runner_, true, 0, callback_receiver_.GetCallback());
  task_runner_->RunUntilIdle();
}

#endif  // defined(GOOGLE_CHROME_BUILD)

INSTANTIATE_TEST_CASE_P(UserLevel, GoogleUpdateWinTest, Values(false));

INSTANTIATE_TEST_CASE_P(SystemLevel, GoogleUpdateWinTest, Values(true));
