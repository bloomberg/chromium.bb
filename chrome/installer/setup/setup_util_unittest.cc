// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_util_unittest.h"

#include <windows.h>

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SetupUtilTestWithDir : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    // Create a temp directory for testing.
    ASSERT_TRUE(test_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    // Clean up test directory manually so we can fail if it leaks.
    ASSERT_TRUE(test_dir_.Delete());
  }

  // The temporary directory used to contain the test operations.
  base::ScopedTempDir test_dir_;
};

// The privilege tested in ScopeTokenPrivilege tests below.
// Use SE_RESTORE_NAME as it is one of the many privileges that is available,
// but not enabled by default on processes running at high integrity.
static const wchar_t kTestedPrivilege[] = SE_RESTORE_NAME;

// Returns true if the current process' token has privilege |privilege_name|
// enabled.
bool CurrentProcessHasPrivilege(const wchar_t* privilege_name) {
  HANDLE temp_handle;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY,
                          &temp_handle)) {
    ADD_FAILURE();
    return false;
  }

  base::win::ScopedHandle token(temp_handle);

  // First get the size of the buffer needed for |privileges| below.
  DWORD size;
  EXPECT_FALSE(::GetTokenInformation(token, TokenPrivileges, NULL, 0, &size));

  scoped_ptr<BYTE[]> privileges_bytes(new BYTE[size]);
  TOKEN_PRIVILEGES* privileges =
      reinterpret_cast<TOKEN_PRIVILEGES*>(privileges_bytes.get());

  if (!::GetTokenInformation(token, TokenPrivileges, privileges, size, &size)) {
    ADD_FAILURE();
    return false;
  }

  // There is no point getting a buffer to store more than |privilege_name|\0 as
  // anything longer will obviously not be equal to |privilege_name|.
  const DWORD desired_size = wcslen(privilege_name);
  const DWORD buffer_size = desired_size + 1;
  scoped_ptr<wchar_t[]> name_buffer(new wchar_t[buffer_size]);
  for (int i = privileges->PrivilegeCount - 1; i >= 0 ; --i) {
    LUID_AND_ATTRIBUTES& luid_and_att = privileges->Privileges[i];
    DWORD size = buffer_size;
    ::LookupPrivilegeName(NULL, &luid_and_att.Luid, name_buffer.get(), &size);
    if (size == desired_size &&
        wcscmp(name_buffer.get(), privilege_name) == 0) {
      return luid_and_att.Attributes == SE_PRIVILEGE_ENABLED;
    }
  }
  return false;
}

}  // namespace

// Test that we are parsing Chrome version correctly.
TEST_F(SetupUtilTestWithDir, GetMaxVersionFromArchiveDirTest) {
  // Create a version dir
  base::FilePath chrome_dir = test_dir_.path().AppendASCII("1.0.0.0");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  scoped_ptr<Version> version(
      installer::GetMaxVersionFromArchiveDir(test_dir_.path()));
  ASSERT_EQ(version->GetString(), "1.0.0.0");

  base::DeleteFile(chrome_dir, true);
  ASSERT_FALSE(base::PathExists(chrome_dir));
  ASSERT_TRUE(installer::GetMaxVersionFromArchiveDir(test_dir_.path()) == NULL);

  chrome_dir = test_dir_.path().AppendASCII("ABC");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  ASSERT_TRUE(installer::GetMaxVersionFromArchiveDir(test_dir_.path()) == NULL);

  chrome_dir = test_dir_.path().AppendASCII("2.3.4.5");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir_.path()));
  ASSERT_EQ(version->GetString(), "2.3.4.5");

  // Create multiple version dirs, ensure that we select the greatest.
  chrome_dir = test_dir_.path().AppendASCII("9.9.9.9");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));
  chrome_dir = test_dir_.path().AppendASCII("1.1.1.1");
  base::CreateDirectory(chrome_dir);
  ASSERT_TRUE(base::PathExists(chrome_dir));

  version.reset(installer::GetMaxVersionFromArchiveDir(test_dir_.path()));
  ASSERT_EQ(version->GetString(), "9.9.9.9");
}

TEST_F(SetupUtilTestWithDir, DeleteFileFromTempProcess) {
  base::FilePath test_file;
  base::CreateTemporaryFileInDir(test_dir_.path(), &test_file);
  ASSERT_TRUE(base::PathExists(test_file));
  base::WriteFile(test_file, "foo", 3);
  EXPECT_TRUE(installer::DeleteFileFromTempProcess(test_file, 0));
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(base::PathExists(test_file));
}

// Note: This test is only valid when run at high integrity (i.e. it will fail
// at medium integrity).
TEST(SetupUtilTest, ScopedTokenPrivilegeBasic) {
  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));

  {
    installer::ScopedTokenPrivilege test_scoped_privilege(kTestedPrivilege);
    ASSERT_TRUE(test_scoped_privilege.is_enabled());
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
  }

  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));
}

// Note: This test is only valid when run at high integrity (i.e. it will fail
// at medium integrity).
TEST(SetupUtilTest, ScopedTokenPrivilegeAlreadyEnabled) {
  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));

  {
    installer::ScopedTokenPrivilege test_scoped_privilege(kTestedPrivilege);
    ASSERT_TRUE(test_scoped_privilege.is_enabled());
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
    {
      installer::ScopedTokenPrivilege dup_scoped_privilege(kTestedPrivilege);
      ASSERT_TRUE(dup_scoped_privilege.is_enabled());
      ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
    }
    ASSERT_TRUE(CurrentProcessHasPrivilege(kTestedPrivilege));
  }

  ASSERT_FALSE(CurrentProcessHasPrivilege(kTestedPrivilege));
}

const char kAdjustProcessPriority[] = "adjust-process-priority";

PriorityClassChangeResult DoProcessPriorityAdjustment() {
  return installer::AdjustProcessPriority() ? PCCR_CHANGED : PCCR_UNCHANGED;
}

namespace {

// A scoper that sets/resets the current process's priority class.
class ScopedPriorityClass {
 public:
  // Applies |priority_class|, returning an instance if a change was made.
  // Otherwise, returns an empty scoped_ptr.
  static scoped_ptr<ScopedPriorityClass> Create(DWORD priority_class);
  ~ScopedPriorityClass();

 private:
  explicit ScopedPriorityClass(DWORD original_priority_class);
  DWORD original_priority_class_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPriorityClass);
};

scoped_ptr<ScopedPriorityClass> ScopedPriorityClass::Create(
    DWORD priority_class) {
  HANDLE this_process = ::GetCurrentProcess();
  DWORD original_priority_class = ::GetPriorityClass(this_process);
  EXPECT_NE(0U, original_priority_class);
  if (original_priority_class && original_priority_class != priority_class) {
    BOOL result = ::SetPriorityClass(this_process, priority_class);
    EXPECT_NE(FALSE, result);
    if (result) {
      return scoped_ptr<ScopedPriorityClass>(
          new ScopedPriorityClass(original_priority_class));
    }
  }
  return scoped_ptr<ScopedPriorityClass>();
}

ScopedPriorityClass::ScopedPriorityClass(DWORD original_priority_class)
    : original_priority_class_(original_priority_class) {}

ScopedPriorityClass::~ScopedPriorityClass() {
  BOOL result = ::SetPriorityClass(::GetCurrentProcess(),
                                   original_priority_class_);
  EXPECT_NE(FALSE, result);
}

PriorityClassChangeResult RelaunchAndDoProcessPriorityAdjustment() {
  CommandLine cmd_line(*CommandLine::ForCurrentProcess());
  cmd_line.AppendSwitch(kAdjustProcessPriority);
  base::ProcessHandle process_handle = NULL;
  int exit_code = 0;
  if (!base::LaunchProcess(cmd_line, base::LaunchOptions(),
                           &process_handle)) {
    ADD_FAILURE() << " to launch subprocess.";
  } else if (!base::WaitForExitCode(process_handle, &exit_code)) {
    ADD_FAILURE() << " to wait for subprocess to exit.";
  } else {
    return static_cast<PriorityClassChangeResult>(exit_code);
  }
  return PCCR_UNKNOWN;
}

}  // namespace

// Launching a subprocess at normal priority class is a noop.
TEST(SetupUtilTest, AdjustFromNormalPriority) {
  ASSERT_EQ(NORMAL_PRIORITY_CLASS, ::GetPriorityClass(::GetCurrentProcess()));
  EXPECT_EQ(PCCR_UNCHANGED, RelaunchAndDoProcessPriorityAdjustment());
}

// Launching a subprocess below normal priority class drops it to bg mode for
// sufficiently recent operating systems.
TEST(SetupUtilTest, AdjustFromBelowNormalPriority) {
  scoped_ptr<ScopedPriorityClass> below_normal =
      ScopedPriorityClass::Create(BELOW_NORMAL_PRIORITY_CLASS);
  ASSERT_TRUE(below_normal);
  if (base::win::GetVersion() > base::win::VERSION_SERVER_2003)
    EXPECT_EQ(PCCR_CHANGED, RelaunchAndDoProcessPriorityAdjustment());
  else
    EXPECT_EQ(PCCR_UNCHANGED, RelaunchAndDoProcessPriorityAdjustment());
}

namespace {

// A test fixture that configures an InstallationState and an InstallerState
// with a product being updated.
class FindArchiveToPatchTest : public SetupUtilTestWithDir {
 protected:
  class FakeInstallationState : public installer::InstallationState {
  };

  class FakeProductState : public installer::ProductState {
   public:
    static FakeProductState* FromProductState(const ProductState* product) {
      return static_cast<FakeProductState*>(const_cast<ProductState*>(product));
    }

    void set_version(const Version& version) {
      if (version.IsValid())
        version_.reset(new Version(version));
      else
        version_.reset();
    }

    void set_uninstall_command(const CommandLine& uninstall_command) {
      uninstall_command_ = uninstall_command;
    }
  };

  virtual void SetUp() OVERRIDE {
    SetupUtilTestWithDir::SetUp();
    product_version_ = Version("30.0.1559.0");
    max_version_ = Version("47.0.1559.0");

    // Install the product according to the version.
    original_state_.reset(new FakeInstallationState());
    InstallProduct();

    // Prepare to update the product in the temp dir.
    installer_state_.reset(new installer::InstallerState(
        kSystemInstall_ ? installer::InstallerState::SYSTEM_LEVEL :
        installer::InstallerState::USER_LEVEL));
    installer_state_->AddProductFromState(
        kProductType_,
        *original_state_->GetProductState(kSystemInstall_, kProductType_));

    // Create archives in the two version dirs.
    ASSERT_TRUE(
        base::CreateDirectory(GetProductVersionArchivePath().DirName()));
    ASSERT_EQ(1, base::WriteFile(GetProductVersionArchivePath(), "a", 1));
    ASSERT_TRUE(
        base::CreateDirectory(GetMaxVersionArchivePath().DirName()));
    ASSERT_EQ(1, base::WriteFile(GetMaxVersionArchivePath(), "b", 1));
  }

  virtual void TearDown() OVERRIDE {
    original_state_.reset();
    SetupUtilTestWithDir::TearDown();
  }

  base::FilePath GetArchivePath(const Version& version) const {
    return test_dir_.path()
        .AppendASCII(version.GetString())
        .Append(installer::kInstallerDir)
        .Append(installer::kChromeArchive);
  }

  base::FilePath GetMaxVersionArchivePath() const {
    return GetArchivePath(max_version_);
  }

  base::FilePath GetProductVersionArchivePath() const {
    return GetArchivePath(product_version_);
  }

  void InstallProduct() {
    FakeProductState* product = FakeProductState::FromProductState(
        original_state_->GetNonVersionedProductState(kSystemInstall_,
                                                     kProductType_));

    product->set_version(product_version_);
    CommandLine uninstall_command(
        test_dir_.path().AppendASCII(product_version_.GetString())
        .Append(installer::kInstallerDir)
        .Append(installer::kSetupExe));
    uninstall_command.AppendSwitch(installer::switches::kUninstall);
    product->set_uninstall_command(uninstall_command);
  }

  void UninstallProduct() {
    FakeProductState::FromProductState(
        original_state_->GetNonVersionedProductState(kSystemInstall_,
                                                     kProductType_))
        ->set_version(Version());
  }

  static const bool kSystemInstall_;
  static const BrowserDistribution::Type kProductType_;
  Version product_version_;
  Version max_version_;
  scoped_ptr<FakeInstallationState> original_state_;
  scoped_ptr<installer::InstallerState> installer_state_;
};

const bool FindArchiveToPatchTest::kSystemInstall_ = false;
const BrowserDistribution::Type FindArchiveToPatchTest::kProductType_ =
    BrowserDistribution::CHROME_BROWSER;

}  // namespace

// Test that the path to the advertised product version is found.
TEST_F(FindArchiveToPatchTest, ProductVersionFound) {
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_));
  EXPECT_EQ(GetProductVersionArchivePath().value(), patch_source.value());
}

// Test that the path to the max version is found if the advertised version is
// missing.
TEST_F(FindArchiveToPatchTest, MaxVersionFound) {
  // The patch file is absent.
  ASSERT_TRUE(base::DeleteFile(GetProductVersionArchivePath(), false));
  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_));
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source.value());

  // The product doesn't appear to be installed, so the max version is found.
  UninstallProduct();
  patch_source = installer::FindArchiveToPatch(
      *original_state_, *installer_state_);
  EXPECT_EQ(GetMaxVersionArchivePath().value(), patch_source.value());
}

// Test that an empty path is returned if no version is found.
TEST_F(FindArchiveToPatchTest, NoVersionFound) {
  // The product doesn't appear to be installed and no archives are present.
  UninstallProduct();
  ASSERT_TRUE(base::DeleteFile(GetProductVersionArchivePath(), false));
  ASSERT_TRUE(base::DeleteFile(GetMaxVersionArchivePath(), false));

  base::FilePath patch_source(installer::FindArchiveToPatch(
      *original_state_, *installer_state_));
  EXPECT_EQ(base::FilePath::StringType(), patch_source.value());
}

namespace {

class MigrateMultiToSingleTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    registry_override_manager_.OverrideRegistry(kRootKey,
                                                L"MigrateMultiToSingleTest");
  }

  static const bool kSystemLevel = false;
  static const HKEY kRootKey;
  static const wchar_t kVersionString[];
  static const wchar_t kMultiChannel[];
  registry_util::RegistryOverrideManager registry_override_manager_;
};

const bool MigrateMultiToSingleTest::kSystemLevel;
const HKEY MigrateMultiToSingleTest::kRootKey =
    kSystemLevel ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
const wchar_t MigrateMultiToSingleTest::kVersionString[] = L"30.0.1574.0";
const wchar_t MigrateMultiToSingleTest::kMultiChannel[] =
    L"2.0-dev-multi-chromeframe";

}  // namespace

// Test migrating Chrome Frame from multi to single.
TEST_F(MigrateMultiToSingleTest, ChromeFrame) {
  installer::ProductState chrome_frame;
  installer::ProductState binaries;
  DWORD usagestats = 0;

  // Set up a config with dev-channel multi-install GCF.
  base::win::RegKey key;

  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  ASSERT_EQ(ERROR_SUCCESS,
            base::win::RegKey(kRootKey, dist->GetVersionKey().c_str(),
                              KEY_SET_VALUE)
                .WriteValue(google_update::kRegVersionField, kVersionString));
  ASSERT_EQ(ERROR_SUCCESS,
            base::win::RegKey(kRootKey, dist->GetStateKey().c_str(),
                              KEY_SET_VALUE)
                .WriteValue(google_update::kRegApField, kMultiChannel));
  ASSERT_EQ(ERROR_SUCCESS,
            base::win::RegKey(kRootKey, dist->GetStateKey().c_str(),
                              KEY_SET_VALUE)
                .WriteValue(google_update::kRegUsageStatsField, 1U));

  dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_FRAME);
  ASSERT_EQ(ERROR_SUCCESS,
            base::win::RegKey(kRootKey, dist->GetVersionKey().c_str(),
                              KEY_SET_VALUE)
                .WriteValue(google_update::kRegVersionField, kVersionString));
  ASSERT_EQ(ERROR_SUCCESS,
            base::win::RegKey(kRootKey, dist->GetStateKey().c_str(),
                              KEY_SET_VALUE)
                .WriteValue(google_update::kRegApField, kMultiChannel));

  // Do the registry migration.
  installer::InstallationState machine_state;
  machine_state.Initialize();

  installer::MigrateGoogleUpdateStateMultiToSingle(
      kSystemLevel,
      BrowserDistribution::CHROME_FRAME,
      machine_state);

  // Confirm that usagestats were copied to CF and that its channel was
  // stripped.
  ASSERT_TRUE(chrome_frame.Initialize(kSystemLevel,
                                      BrowserDistribution::CHROME_FRAME));
  EXPECT_TRUE(chrome_frame.GetUsageStats(&usagestats));
  EXPECT_EQ(1U, usagestats);
  EXPECT_EQ(L"2.0-dev", chrome_frame.channel().value());

  // Confirm that the binaries' channel no longer contains GCF.
  ASSERT_TRUE(binaries.Initialize(kSystemLevel,
                                  BrowserDistribution::CHROME_BINARIES));
  EXPECT_EQ(L"2.0-dev-multi", binaries.channel().value());
}

TEST(SetupUtilTest, ContainsUnsupportedSwitch) {
  EXPECT_FALSE(installer::ContainsUnsupportedSwitch(
      CommandLine::FromString(L"foo.exe")));
  EXPECT_FALSE(installer::ContainsUnsupportedSwitch(
      CommandLine::FromString(L"foo.exe --multi-install --chrome")));
  EXPECT_TRUE(installer::ContainsUnsupportedSwitch(
      CommandLine::FromString(L"foo.exe --chrome-frame")));
}
