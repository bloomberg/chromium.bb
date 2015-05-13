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
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "google_update/google_update_idl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"
#include "version.h"

using ::testing::Assign;
using ::testing::DoAll;
using ::testing::DoubleEq;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Values;
using ::testing::_;

namespace {

class MockUpdateCheckDelegate : public UpdateCheckDelegate {
 public:
  MockUpdateCheckDelegate() : weak_ptr_factory_(this) {}

  base::WeakPtr<UpdateCheckDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD1(OnUpdateCheckComplete, void(const base::string16&));
  MOCK_METHOD2(OnUpgradeProgress, void(int, const base::string16&));
  MOCK_METHOD1(OnUpgradeComplete, void(const base::string16&));
  MOCK_METHOD3(OnError, void(GoogleUpdateErrorCode,
                             const base::string16&,
                             const base::string16&));

 private:
  base::WeakPtrFactory<UpdateCheckDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockUpdateCheckDelegate);
};

// An interface that exposes a factory method for creating an IGoogleUpdate3Web
// instance.
class GoogleUpdateFactory {
 public:
  virtual ~GoogleUpdateFactory() {}
  virtual HRESULT Create(
      base::win::ScopedComPtr<IGoogleUpdate3Web>* google_update) = 0;
};

// A mock factory for creating an IGoogleUpdate3Web instance.
class MockGoogleUpdateFactory : public GoogleUpdateFactory {
 public:
  MockGoogleUpdateFactory() {}
  MOCK_METHOD1(Create, HRESULT(base::win::ScopedComPtr<IGoogleUpdate3Web>*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGoogleUpdateFactory);
};

class MockCurrentState : public CComObjectRootEx<CComSingleThreadModel>,
                         public ICurrentState {
 public:
  BEGIN_COM_MAP(MockCurrentState)
    COM_INTERFACE_ENTRY(ICurrentState)
  END_COM_MAP()

  MockCurrentState() {}

  // Adds an expectation for get_completionMessage that will return the given
  // message any number of times.
  void ExpectCompletionMessage(const base::string16& completion_message) {
    completion_message_ = completion_message;
    EXPECT_CALL(*this, get_completionMessage(_))
        .WillRepeatedly(
            ::testing::Invoke(this, &MockCurrentState::GetCompletionMessage));
  }

  HRESULT GetCompletionMessage(BSTR* completion_message) {
    *completion_message = SysAllocString(completion_message_.c_str());
    return S_OK;
  }

  // Adds an expectation for get_availableVersion that will return the given
  // version any number of times.
  void ExpectAvailableVersion(const base::string16& available_version) {
    available_version_ = available_version;
    EXPECT_CALL(*this, get_availableVersion(_))
        .WillRepeatedly(
            ::testing::Invoke(this, &MockCurrentState::GetAvailableVersion));
  }

  HRESULT GetAvailableVersion(BSTR* available_version) {
    *available_version = SysAllocString(available_version_.c_str());
    return S_OK;
  }

  // ICurrentState:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_stateValue,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_availableVersion,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_bytesDownloaded,
                             HRESULT(ULONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_totalBytesToDownload,
                             HRESULT(ULONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_downloadTimeRemainingMs,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_nextRetryTime,
                             HRESULT(ULONGLONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installProgress,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installTimeRemainingMs,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_isCanceled,
                             HRESULT(VARIANT_BOOL *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_errorCode,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_extraCode1,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_completionMessage,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installerResultCode,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installerResultExtraCode1,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_postInstallLaunchCommandLine,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_postInstallUrl,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_postInstallAction,
                             HRESULT(LONG *));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

 private:
  base::string16 completion_message_;
  base::string16 available_version_;

  DISALLOW_COPY_AND_ASSIGN(MockCurrentState);
};

class MockApp : public CComObjectRootEx<CComSingleThreadModel>, public IAppWeb {
 public:
  BEGIN_COM_MAP(MockApp)
    COM_INTERFACE_ENTRY(IAppWeb)
  END_COM_MAP()

  MockApp() {}

  // IAppWeb:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_appId,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_currentVersionWeb,
                             HRESULT(IDispatch **));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_nextVersionWeb,
                             HRESULT(IDispatch **));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_command,
                             HRESULT(BSTR, IDispatch **));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             cancel,
                             HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_currentState,
                             HRESULT(IDispatch **));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             launch,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             uninstall,
                             HRESULT());

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockApp);
};

class MockAppBundle : public CComObjectRootEx<CComSingleThreadModel>,
                      public IAppBundleWeb {
 public:
  BEGIN_COM_MAP(MockAppBundle)
    COM_INTERFACE_ENTRY(IAppBundleWeb)
  END_COM_MAP()

  MockAppBundle() {}

  // IAppBundleWeb:
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createApp,
                             HRESULT(BSTR, BSTR, BSTR, BSTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createInstalledApp,
                             HRESULT(BSTR));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createAllInstalledApps,
                             HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_displayLanguage,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             put_displayLanguage,
                             HRESULT(BSTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             put_parentHWND,
                             HRESULT(ULONG_PTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_length,
                             HRESULT(int *));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_appWeb,
                             HRESULT(int, IDispatch **));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             initialize,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             checkForUpdate,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             download,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             install,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             pause,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             resume,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             cancel,
                             HRESULT());
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             downloadPackage,
                             HRESULT(BSTR, BSTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_currentState,
                             HRESULT(VARIANT *));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAppBundle);
};

class MockGoogleUpdate : public CComObjectRootEx<CComSingleThreadModel>,
                         public IGoogleUpdate3Web {
 public:
  BEGIN_COM_MAP(MockGoogleUpdate)
    COM_INTERFACE_ENTRY(IGoogleUpdate3Web)
  END_COM_MAP()

  MockGoogleUpdate() {}

  // IGoogleUpdate3Web:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createAppBundleWeb,
                             HRESULT(IDispatch**));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

  DISALLOW_COPY_AND_ASSIGN(MockGoogleUpdate);
};

}  // namespace

// A test fixture that can simulate the IGoogleUpdate3Web API via Google Mock.
// Individual tests must wire up the factories by a call to one of the
// PrepareSimulator methods. The family of Push*State methods are then used to
// configure the set of states to be simulated.
class GoogleUpdateWinTest : public ::testing::TestWithParam<bool> {
 public:
  static void SetUpTestCase() {
    ui::win::CreateATLModuleIfNeeded();
    // Configure all mock functions that return HRESULT to return failure.
    ::testing::DefaultValue<HRESULT>::Set(E_FAIL);
  }

  static void TearDownTestCase() { ::testing::DefaultValue<HRESULT>::Clear(); }

 protected:
  GoogleUpdateWinTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_handle_(task_runner_),
        system_level_install_(GetParam()),
        mock_app_(nullptr),
        mock_app_bundle_(nullptr) {}

  void SetUp() override {
    ::testing::TestWithParam<bool>::SetUp();

    // Override FILE_EXE so that it looks like the test is running from the
    // standard install location for this mode (system-level or user-level).
    base::FilePath file_exe;
    ASSERT_TRUE(PathService::Get(base::FILE_EXE, &file_exe));
    base::FilePath install_dir(installer::GetChromeInstallPath(
        system_level_install_, BrowserDistribution::GetDistribution()));
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
    const HKEY root =
        system_level_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
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

    // Provide an IGoogleUpdate3Web class factory so that this test can provide
    // a mocked-out instance.
    SetGoogleUpdateFactoryForTesting(
        base::Bind(&GoogleUpdateFactory::Create,
                   base::Unretained(&mock_google_update_factory_)));

    // Compute a newer version.
    base::Version current_version(CHROME_VERSION_STRING);
    new_version_ = base::StringPrintf(L"%u.%u.%u.%u",
                                      current_version.components()[0],
                                      current_version.components()[1],
                                      current_version.components()[2] + 1,
                                      current_version.components()[3]);
  }

  void TearDown() override {
    ASSERT_EQ(nullptr, mock_app_);
    ASSERT_EQ(nullptr, mock_app_bundle_);
    ASSERT_TRUE(states_.empty());
    // Remove the test's IGoogleUpdate on-demand update class factory.
    SetGoogleUpdateFactoryForTesting(GoogleUpdate3ClassFactory());
    ::testing::TestWithParam<bool>::TearDown();
  }

  // Prepares the IGoogleUpdate3Web simulator.
  void PrepareSimulator(const base::char16* app_guid) {
    // Make the installed app and configure it to call the test fixture's
    // GetNextState method.
    ASSERT_EQ(S_OK, CComObject<MockApp>::CreateInstance(&mock_app_));
    ON_CALL(*mock_app_, get_currentState(_))
        .WillByDefault(Invoke(this, &GoogleUpdateWinTest::GetNextState));

    // Make the app bundle containing the app.
    ASSERT_EQ(S_OK,
              CComObject<MockAppBundle>::CreateInstance(&mock_app_bundle_));
    EXPECT_CALL(*mock_app_bundle_, initialize())
        .WillOnce(Return(S_OK));
    // The bundle will be called on to create the installed app.
    EXPECT_CALL(*mock_app_bundle_, createInstalledApp(StrEq(app_guid)))
        .WillOnce(Return(S_OK));
    // Give mock_app_bundle a ref to the app which it will return when asked.
    mock_app_->AddRef();
    EXPECT_CALL(*mock_app_bundle_, get_appWeb(0, _))
        .WillOnce(DoAll(SetArgPointee<1>(mock_app_),
                        Assign(&mock_app_, nullptr),
                        Return(S_OK)));

    // Make the updater which will return the app bundle above.
    CComObject<MockGoogleUpdate>* mock_google_update = nullptr;
    ASSERT_EQ(S_OK, CComObject<MockGoogleUpdate>::CreateInstance(
                        &mock_google_update));
    // Give mock_google_update a ref to the bundle which it will return when
    // created.
    mock_app_bundle_->AddRef();
    EXPECT_CALL(*mock_google_update, createAppBundleWeb(_))
        .WillOnce(DoAll(SetArgPointee<0>(mock_app_bundle_),
                        Assign(&mock_app_bundle_, nullptr),
                        Return(S_OK)));

    // Give the factory this updater. Do not add a ref, as the factory will add
    // one when it hands out its instance.
    EXPECT_CALL(mock_google_update_factory_, Create(_))
        .WillOnce(DoAll(SetArgPointee<0>(mock_google_update), Return(S_OK)));
  }

  // Prepares the IGoogleUpdate3Web simulator to process an update check.
  void PrepareSimulatorForUpdateCheck(const base::char16* app_guid) {
    PrepareSimulator(app_guid);
    // Expect the bundle to be called on to start the update.
    EXPECT_CALL(*mock_app_bundle_, checkForUpdate())
        .WillOnce(Return(S_OK));
  }

  // Prepares the IGoogleUpdate3Web simulator to process an upgrade.
  void PrepareSimulatorForUpgrade(const base::char16* app_guid) {
    PrepareSimulatorForUpdateCheck(app_guid);
    // Expect the bundle to be called on to start the install.
    EXPECT_CALL(*mock_app_bundle_, install())
        .WillOnce(Return(S_OK));
  }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb.
  void PushState(CurrentState state) {
    MakeNextState(state);
  }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb for an ERROR state.
  void PushErrorState(LONG error_code,
                      const base::string16& completion_message,
                      LONG installer_result_code) {
    CComObject<MockCurrentState>* mock_state = MakeNextState(STATE_ERROR);
    EXPECT_CALL(*mock_state, get_errorCode(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(error_code), Return(S_OK)));
    mock_state->ExpectCompletionMessage(completion_message);
    if (installer_result_code != -1) {
      EXPECT_CALL(*mock_state, get_installerResultCode(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(installer_result_code),
                                Return(S_OK)));
    }
  }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb for an UPDATE_AVAILABLE state.
  void PushUpdateAvailableState(const base::string16& new_version) {
    MakeNextState(STATE_UPDATE_AVAILABLE)->ExpectAvailableVersion(new_version);
  }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb for a DOWNLOADING or INSTALLING state.
  void PushProgressiveState(CurrentState state, int progress) {
    CComObject<MockCurrentState>* mock_state = MakeNextState(state);
    if (state == STATE_DOWNLOADING) {
      const ULONG kTotalBytes = 1024;
      ULONG bytes_down = static_cast<double>(kTotalBytes) * progress / 100.0;
      EXPECT_CALL(*mock_state, get_totalBytesToDownload(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(kTotalBytes), Return(S_OK)));
      EXPECT_CALL(*mock_state, get_bytesDownloaded(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(bytes_down), Return(S_OK)));
    } else if (state == STATE_INSTALLING) {
      EXPECT_CALL(*mock_state, get_installProgress(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(progress), Return(S_OK)));
    } else {
      ADD_FAILURE() << "unsupported state " << state;
    }
  }

  static const base::char16 kClients[];
  static const base::char16 kClientState[];
  static const base::char16 kChromeGuid[];
  static const base::char16 kChromeBinariesGuid[];

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  bool system_level_install_;
  scoped_ptr<base::ScopedPathOverride> file_exe_override_;
  scoped_ptr<base::ScopedPathOverride> program_files_override_;
  scoped_ptr<base::ScopedPathOverride> program_files_x86_override_;
  scoped_ptr<base::ScopedPathOverride> local_app_data_override_;
  registry_util::RegistryOverrideManager registry_override_manager_;

  // A mock object, the OnUpdateCheckCallback method of which will be invoked
  // each time the update check machinery invokes the given UpdateCheckCallback.
  StrictMock<MockUpdateCheckDelegate> mock_update_check_delegate_;

  // A mock object that provides a GoogleUpdate3ClassFactory by which the test
  // fixture's IGoogleUpdate3Web simulator is provided to the update check
  // machinery.
  StrictMock<MockGoogleUpdateFactory> mock_google_update_factory_;

  // The mock objects that simulate the IGoogleUpdate3Web API.
  CComObject<MockApp>* mock_app_;
  CComObject<MockAppBundle>* mock_app_bundle_;

  // A gmock sequence under which expectations on the MockApp are evaluated.
  Sequence state_sequence_;

  // The states returned by the MockApp when probed.
  std::queue<CComObject<MockCurrentState>*> states_;

  // The new version that the fixture will pretend is available.
  base::string16 new_version_;

 private:
  // Returns a new MockCurrentState that will be returned by the mock IAppWeb's
  // get_currentState method.
  CComObject<MockCurrentState>* MakeNextState(CurrentState state) {
    CComObject<MockCurrentState>* mock_state = nullptr;
    // The new object's refcount is held at zero until it is released from the
    // simulator in GetNextState.
    EXPECT_EQ(S_OK, CComObject<MockCurrentState>::CreateInstance(&mock_state));
    EXPECT_CALL(*mock_state, get_stateValue(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(state), Return(S_OK)));
    states_.push(mock_state);
    // Tell the app to expect this state.
    EXPECT_CALL(*mock_app_, get_currentState(_))
        .InSequence(state_sequence_);
    return mock_state;
  }

  // An implementation of IAppWeb::get_currentState that advances the
  // IGoogleUpdate3Web simulator through a series of states.
  HRESULT GetNextState(IDispatch** current_state) {
    EXPECT_FALSE(states_.empty());
    *current_state = states_.front();
    // Give a reference to the caller.
    (*current_state)->AddRef();
    states_.pop();
    return S_OK;
  }

  DISALLOW_COPY_AND_ASSIGN(GoogleUpdateWinTest);
};

//  static
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

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY, _, _));
  BeginUpdateCheck(task_runner_, std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

#if defined(GOOGLE_CHROME_BUILD)

// Test the case where the GoogleUpdate class can't be created for an update
// check.
TEST_P(GoogleUpdateWinTest, NoGoogleUpdateForCheck) {
  // The factory should be called upon: let it fail.
  EXPECT_CALL(mock_google_update_factory_, Create(_));

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, _, _));
  BeginUpdateCheck(task_runner_, std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test the case where the GoogleUpdate class can't be created for an upgrade.
TEST_P(GoogleUpdateWinTest, NoGoogleUpdateForUpgrade) {
  // The factory should be called upon: let it fail.
  EXPECT_CALL(mock_google_update_factory_, Create(_));

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, _, _));
  BeginUpdateCheck(task_runner_, std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test the case where the GoogleUpdate class returns an error when an update
// check is started.
TEST_P(GoogleUpdateWinTest, FailUpdateCheck) {
  // The factory should be called upon: let it return the mock on-demand class.
  PrepareSimulator(kChromeBinariesGuid);
  // checkForUpdate will fail.
  EXPECT_CALL(*mock_app_bundle_, checkForUpdate())
      .WillOnce(Return(E_FAIL));

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR, _, _));
  BeginUpdateCheck(task_runner_, std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test the case where the GoogleUpdate class reports that updates are disabled
// by Group Policy.
TEST_P(GoogleUpdateWinTest, UpdatesDisabledByPolicy) {
  static const HRESULT GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY = 0x80040813;
  PrepareSimulatorForUpdateCheck(kChromeBinariesGuid);
  PushState(STATE_INIT);
  PushState(STATE_CHECKING_FOR_UPDATE);
  PushErrorState(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY,
                 L"disabled by policy", -1);

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_DISABLED_BY_POLICY, _, _));
  BeginUpdateCheck(task_runner_, std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test an update check where no update is available.
TEST_P(GoogleUpdateWinTest, UpdateCheckNoUpdate) {
  PrepareSimulatorForUpdateCheck(kChromeBinariesGuid);
  PushState(STATE_INIT);
  PushState(STATE_CHECKING_FOR_UPDATE);
  PushState(STATE_NO_UPDATE);

  EXPECT_CALL(mock_update_check_delegate_,
              OnUpdateCheckComplete(IsEmpty()));  // new_version
  BeginUpdateCheck(task_runner_, std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test an update check where an update is available.
TEST_P(GoogleUpdateWinTest, UpdateCheckUpdateAvailable) {
  PrepareSimulatorForUpdateCheck(kChromeBinariesGuid);
  PushState(STATE_INIT);
  PushState(STATE_CHECKING_FOR_UPDATE);
  PushUpdateAvailableState(new_version_);

  EXPECT_CALL(mock_update_check_delegate_,
              OnUpdateCheckComplete(StrEq(new_version_)));
  BeginUpdateCheck(task_runner_, std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test a successful upgrade.
TEST_P(GoogleUpdateWinTest, UpdateInstalled) {
  PrepareSimulatorForUpgrade(kChromeBinariesGuid);
  PushState(STATE_INIT);
  PushState(STATE_CHECKING_FOR_UPDATE);
  PushUpdateAvailableState(new_version_);
  PushState(STATE_WAITING_TO_DOWNLOAD);
  PushProgressiveState(STATE_DOWNLOADING, 0);
  PushProgressiveState(STATE_DOWNLOADING, 25);
  PushProgressiveState(STATE_DOWNLOADING, 25);
  PushProgressiveState(STATE_DOWNLOADING, 75);
  PushState(STATE_WAITING_TO_INSTALL);
  PushProgressiveState(STATE_INSTALLING, 50);
  PushState(STATE_INSTALL_COMPLETE);

  {
    InSequence callback_sequence;
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(0, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(12, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(37, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(50, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(75, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeComplete(StrEq(new_version_)));
  }
  BeginUpdateCheck(task_runner_, std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

// Test a failed upgrade where Google Update reports that the installer failed.
TEST_P(GoogleUpdateWinTest, UpdateFailed) {
  const base::string16 error(L"It didn't work.");
  static const HRESULT GOOPDATEINSTALL_E_INSTALLER_FAILED = 0x80040902;
  static const int kInstallerError = 12;

  PrepareSimulatorForUpgrade(kChromeBinariesGuid);
  PushState(STATE_INIT);
  PushState(STATE_CHECKING_FOR_UPDATE);
  PushUpdateAvailableState(new_version_);
  PushState(STATE_WAITING_TO_DOWNLOAD);
  PushProgressiveState(STATE_DOWNLOADING, 0);
  PushProgressiveState(STATE_DOWNLOADING, 25);
  PushProgressiveState(STATE_DOWNLOADING, 25);
  PushProgressiveState(STATE_DOWNLOADING, 75);
  PushState(STATE_WAITING_TO_INSTALL);
  PushProgressiveState(STATE_INSTALLING, 50);
  PushErrorState(GOOPDATEINSTALL_E_INSTALLER_FAILED, error, kInstallerError);

  {
    InSequence callback_sequence;
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(0, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(12, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(37, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(50, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(75, StrEq(new_version_)));
    EXPECT_CALL(mock_update_check_delegate_,
                OnError(GOOGLE_UPDATE_ERROR_UPDATING, HasSubstr(error),
                        StrEq(new_version_)));
  }
  BeginUpdateCheck(task_runner_, std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
}

#endif  // defined(GOOGLE_CHROME_BUILD)

INSTANTIATE_TEST_CASE_P(UserLevel, GoogleUpdateWinTest, Values(false));

INSTANTIATE_TEST_CASE_P(SystemLevel, GoogleUpdateWinTest, Values(true));
