// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace file_manager {

// FileManagerBrowserTest parameters: first controls IN_GUEST_MODE or not, the
// second is the JS test case name.
typedef std::tuple<GuestMode, const char*> TestParameter;

// FileManager browser test class.
class FileManagerBrowserTest :
      public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<TestParameter> {
 public:
  FileManagerBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);

    if (shouldEnableLegacyEventDispatch()) {
      command_line->AppendSwitchASCII("disable-blink-features",
                                      "TrustedEventsDefaultAction");
    }
  }

  GuestMode GetGuestMode() const override {
    return std::get<0>(GetParam());
  }

  const char* GetTestCaseName() const override {
    return std::get<1>(GetParam());
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

 private:
  bool shouldEnableLegacyEventDispatch() {
    const std::string test_case_name = GetTestCaseName();
    // crbug.com/482121 crbug.com/480491
    return test_case_name.find("tabindex") != std::string::npos;
  }

  DISALLOW_COPY_AND_ASSIGN(FileManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTest, Test) {
  StartTest();
}

// FileManager browser test class for tests that rely on deprecated event
// dispatch that send tests.
class FileManagerBrowserTestWithLegacyEventDispatch
    : public FileManagerBrowserTest {
 public:
  FileManagerBrowserTestWithLegacyEventDispatch() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("disable-blink-features",
                                    "TrustedEventsDefaultAction");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FileManagerBrowserTestWithLegacyEventDispatch);
};

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTestWithLegacyEventDispatch, Test) {
  StartTest();
}

// Unlike TEST/TEST_F, which are macros that expand to further macros,
// INSTANTIATE_TEST_CASE_P is a macro that expands directly to code that
// stringizes the arguments. As a result, macros passed as parameters (such as
// prefix or test_case_name) will not be expanded by the preprocessor. To work
// around this, indirect the macro for INSTANTIATE_TEST_CASE_P, so that the
// pre-processor will expand macros such as MAYBE_test_name before
// instantiating the test.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator) \
  INSTANTIATE_TEST_CASE_P(prefix, test_case_name, generator)

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FileDisplay,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "fileDisplayDownloads"),
                      TestParameter(IN_GUEST_MODE, "fileDisplayDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileDisplayDrive"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileDisplayMtp"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileSearch"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileSearchCaseInsensitive"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileSearchNotFound")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenVideoFiles,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "videoOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "videoOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "videoOpenDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenAudioFiles,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(IN_GUEST_MODE, "audioOpenDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "audioOpenDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "audioOpenDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioAutoAdvanceDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioRepeatAllModeSingleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioNoRepeatModeSingleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioRepeatOneModeSingleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioRepeatAllModeMultipleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "audioNoRepeatModeMultipleFileDrive"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "audioRepeatOneModeMultipleFileDrive")));

// Fails on the MSAN bots, https://crbug.com/837551
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenImageFiles DISABLED_OpenImageFiles
#else
#define MAYBE_OpenImageFiles OpenImageFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenImageFiles,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "imageOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "imageOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "imageOpenDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    CreateNewFolder,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "createNewFolderAfterSelectFile"),
        TestParameter(IN_GUEST_MODE, "createNewFolderDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "createNewFolderDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "createNewFolderDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    KeyboardOperations,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(IN_GUEST_MODE, "keyboardDeleteDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "keyboardDeleteDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "keyboardDeleteDrive"),
        TestParameter(IN_GUEST_MODE, "keyboardCopyDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "keyboardCopyDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "keyboardCopyDrive"),
        TestParameter(IN_GUEST_MODE, "renameFileDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "renameFileDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "renameFileDrive"),
        TestParameter(IN_GUEST_MODE, "renameNewFolderDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "renameNewFolderDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "renameNewFolderDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Delete,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE,
                      "deleteMenuItemIsDisabledWhenNoItemIsSelected"),
        TestParameter(NOT_IN_GUEST_MODE, "deleteOneItemFromToolbar")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    QuickView,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "openQuickView"),
                      TestParameter(NOT_IN_GUEST_MODE, "closeQuickView")));
// Disabled due to strong flakyness (crbug.com/798772):
//                      TestParameter(NOT_IN_GUEST_MODE,
//                                    "openQuickViewForFoldersAfterClose")

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DirectoryTreeContextMenu,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE,
                      "copyFromDirectoryTreeWithContextMenu"),
        TestParameter(IN_GUEST_MODE, "copyFromDirectoryTreeWithContextMenu"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "copyFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(IN_GUEST_MODE,
                      "copyFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "copyFromDirectoryTreeWithoutChaningCurrentDirectory"),
        TestParameter(NOT_IN_GUEST_MODE, "cutFromDirectoryTreeWithContextMenu"),
        TestParameter(IN_GUEST_MODE, "cutFromDirectoryTreeWithContextMenu"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "cutFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(IN_GUEST_MODE,
                      "cutFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "cutFromDirectoryTreeWithoutChaningCurrentDirectory"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "pasteIntoFolderFromDirectoryTreeWithContextMenu"),
        TestParameter(IN_GUEST_MODE,
                      "pasteIntoFolderFromDirectoryTreeWithContextMenu"),
        TestParameter(
            NOT_IN_GUEST_MODE,
            "pasteIntoFolderFromDirectoryTreeWithoutChaningCurrentDirectory"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "renameDirectoryFromDirectoryTreeWithContextMenu"),
        TestParameter(IN_GUEST_MODE,
                      "renameDirectoryFromDirectoryTreeWithContextMenu"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "renameDirectoryFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(IN_GUEST_MODE,
                      "renameDirectoryFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(
            NOT_IN_GUEST_MODE,
            "renameDirectoryFromDirectoryTreeWithoutChangingCurrentDirectory"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "renameDirectoryToEmptyStringFromDirectoryTree"),
        TestParameter(IN_GUEST_MODE,
                      "renameDirectoryToEmptyStringFromDirectoryTree"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "renameDirectoryToExistingOneFromDirectoryTree"),
        TestParameter(IN_GUEST_MODE,
                      "renameDirectoryToExistingOneFromDirectoryTree"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "createDirectoryFromDirectoryTreeWithContextMenu"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "createDirectoryFromDirectoryTreeWithKeyboardShortcut"),
        TestParameter(NOT_IN_GUEST_MODE,
                      "createDirectoryFromDirectoryTreeWithoutChangingCurrentDi"
                      "rectory")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DriveSpecific,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarOffline"),
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarSharedWithMe"),
        TestParameter(NOT_IN_GUEST_MODE, "autocomplete"),
        TestParameter(NOT_IN_GUEST_MODE, "pinFileOnMobileNetwork"),
        TestParameter(NOT_IN_GUEST_MODE, "clickFirstSearchResult"),
        TestParameter(NOT_IN_GUEST_MODE, "pressEnterToSearch")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Transfer,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "transferFromDriveToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromDownloadsToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromSharedToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromSharedToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromOfflineToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromOfflineToDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    RestorePrefs,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "restoreSortColumn"),
                      TestParameter(NOT_IN_GUEST_MODE, "restoreSortColumn"),
                      TestParameter(IN_GUEST_MODE, "restoreCurrentView"),
                      TestParameter(NOT_IN_GUEST_MODE, "restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ShareDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "shareFile"),
                      TestParameter(NOT_IN_GUEST_MODE, "shareDirectory")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    RestoreGeometry,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "restoreGeometry"),
                      TestParameter(IN_GUEST_MODE, "restoreGeometry"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "restoreGeometryMaximized")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Traverse,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "traverseDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "traverseDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "traverseDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    SuggestAppDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "suggestAppDialog")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ExecuteDefaultTaskOnDownloads,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "executeDefaultTaskOnDownloads"),
        TestParameter(IN_GUEST_MODE, "executeDefaultTaskOnDownloads")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ExecuteDefaultTaskOnDrive,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "executeDefaultTaskOnDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DefaultTaskDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "defaultTaskDialogOnDownloads"),
        TestParameter(IN_GUEST_MODE, "defaultTaskDialogOnDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "defaultTaskDialogOnDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    GenericTask,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "genericTaskIsNotExecuted"),
        TestParameter(NOT_IN_GUEST_MODE, "genericAndNonGenericTasksAreMixed")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FolderShortcuts,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "traverseFolderShortcuts"),
        TestParameter(NOT_IN_GUEST_MODE, "addRemoveFolderShortcuts")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    SortColumns,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "sortColumns"),
                      TestParameter(IN_GUEST_MODE, "sortColumns")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabIndex,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSearchBoxFocus")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexFocus,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "tabindexFocus")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexFocusDownloads,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "tabindexFocusDownloads"),
                      TestParameter(IN_GUEST_MODE, "tabindexFocusDownloads")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexFocusDirectorySelected,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "tabindexFocusDirectorySelected")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexOpenDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexOpenDialogDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "tabindexOpenDialogDownloads"),
        TestParameter(IN_GUEST_MODE, "tabindexOpenDialogDownloads")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexSaveFileDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSaveFileDialogDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSaveFileDialogDownloads"),
        TestParameter(IN_GUEST_MODE, "tabindexSaveFileDialogDownloads")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenFileDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "openFileDialogOnDownloads"),
                      TestParameter(IN_GUEST_MODE, "openFileDialogOnDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "openFileDialogOnDrive"),
                      TestParameter(IN_INCOGNITO, "openFileDialogOnDownloads"),
                      TestParameter(IN_INCOGNITO, "openFileDialogOnDrive"),
                      TestParameter(NOT_IN_GUEST_MODE, "unloadFileDialog")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_CopyBetweenWindows DISABLED_CopyBetweenWindows
#else
// flaky: http://crbug.com/500966
#define MAYBE_CopyBetweenWindows DISABLED_CopyBetweenWindows
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_CopyBetweenWindows,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsLocalToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsLocalToUsb"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsUsbToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsDriveToLocal"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsDriveToUsb"),
        TestParameter(NOT_IN_GUEST_MODE, "copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ShowGridView,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "showGridViewDownloads"),
                      TestParameter(IN_GUEST_MODE, "showGridViewDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "showGridViewDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Providers,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "requestMount"),
        TestParameter(NOT_IN_GUEST_MODE, "requestMountMultipleMounts"),
        TestParameter(NOT_IN_GUEST_MODE, "requestMountSourceDevice"),
        TestParameter(NOT_IN_GUEST_MODE, "requestMountSourceFile")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    GearMenu,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "showHiddenFilesDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "showHiddenFilesDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "toogleGoogleDocsDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "showPasteIntoCurrentFolder")));

// Structure to describe an account info.
struct TestAccountInfo {
  const char* const gaia_id;
  const char* const email;
  const char* const hash;
  const char* const display_name;
};

enum {
  DUMMY_ACCOUNT_INDEX = 0,
  PRIMARY_ACCOUNT_INDEX = 1,
  SECONDARY_ACCOUNT_INDEX_START = 2,
};

static const TestAccountInfo kTestAccounts[] = {
    {"gaia-id-d", "__dummy__@invalid.domain", "hashdummy", "Dummy Account"},
    {"gaia-id-a", "alice@invalid.domain", "hashalice", "Alice"},
    {"gaia-id-b", "bob@invalid.domain", "hashbob", "Bob"},
    {"gaia-id-c", "charlie@invalid.domain", "hashcharlie", "Charlie"},
};

// Test fixture class for testing multi-profile features.
class MultiProfileFileManagerBrowserTest : public FileManagerBrowserTestBase {
 public:
  MultiProfileFileManagerBrowserTest() = default;

 protected:
  // Enables multi-profiles.
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);
    // Logs in to a dummy profile (For making MultiProfileWindowManager happy;
    // browser test creates a default window and the manager tries to assign a
    // user for it, and we need a profile connected to a user.)
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].email);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile,
                                    kTestAccounts[DUMMY_ACCOUNT_INDEX].hash);
    // Don't require policy for our sessions - this is required because
    // this test creates a secondary profile synchronously, so we need to
    // let the policy code know not to expect cached policy.
    command_line->AppendSwitchASCII(chromeos::switches::kProfileRequiresPolicy,
                                    "false");
  }

  // Logs in to the primary profile of this test.
  void SetUpOnMainThread() override {
    const TestAccountInfo& info = kTestAccounts[PRIMARY_ACCOUNT_INDEX];

    AddUser(info, true);
    FileManagerBrowserTestBase::SetUpOnMainThread();
  }

  // Loads all users to the current session and sets up necessary fields.
  // This is used for preparing all accounts in PRE_ test setup, and for testing
  // actual login behavior.
  void AddAllUsers() {
    for (size_t i = 0; i < arraysize(kTestAccounts); ++i) {
      // The primary account was already set up in SetUpOnMainThread, so skip it
      // here.
      if (i == PRIMARY_ACCOUNT_INDEX)
        continue;
      AddUser(kTestAccounts[i], i >= SECONDARY_ACCOUNT_INDEX_START);
    }
  }

  // Returns primary profile (if it is already created.)
  Profile* profile() override {
    Profile* const profile =
        chromeos::ProfileHelper::GetProfileByUserIdHashForTest(
            kTestAccounts[PRIMARY_ACCOUNT_INDEX].hash);
    return profile ? profile : FileManagerBrowserTestBase::profile();
  }

  // Adds a new user for testing to the current session.
  void AddUser(const TestAccountInfo& info, bool log_in) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(info.email, info.gaia_id));
    if (log_in) {
      session_manager::SessionManager::Get()->CreateSession(account_id,
                                                            info.hash, false);
    }
    user_manager::UserManager::Get()->SaveUserDisplayName(
        account_id, base::UTF8ToUTF16(info.display_name));
    Profile* profile =
        chromeos::ProfileHelper::GetProfileByUserIdHashForTest(info.hash);
    // TODO(https://crbug.com/814307): We can't use
    // identity::MakePrimaryAccountAvailable from identity_test_utils.h here
    // because that DCHECKs that the SigninManager isn't authenticated yet.
    // Here, it *can* be already authenticated if a PRE_ test previously set up
    // the user.
    IdentityManagerFactory::GetForProfile(profile)
        ->SetPrimaryAccountSynchronouslyForTests(info.gaia_id, info.email,
                                                 "refresh_token");
  }

  GuestMode GetGuestMode() const override { return NOT_IN_GUEST_MODE; }

  const char* GetTestCaseName() const override {
    return test_case_name_.c_str();
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileFileManagerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest, PRE_BasicDownloads) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest, BasicDownloads) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest, PRE_BasicDrive) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest, BasicDrive) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDrive");
  StartTest();
}

}  // namespace file_manager
