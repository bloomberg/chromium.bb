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

// TestCase: FileManagerBrowserTest parameters.
struct TestCase {
  explicit TestCase(const char* name)
    : test_name(name) {}

  const char* GetTestName() const {
    CHECK(test_name) << "FATAL: no test name";
    return test_name;
  }

  GuestMode GetGuestMode() const {
    return guest_mode;
  }

  TestCase& InGuestMode() {
    guest_mode = IN_GUEST_MODE;
    return *this;
  }

  TestCase& InIncognito() {
    guest_mode = IN_INCOGNITO;
    return *this;
  }

  const char* test_name = nullptr;
  GuestMode guest_mode = NOT_IN_GUEST_MODE;
};

// FileManager browser test.
class FileManagerBrowserTest : public FileManagerBrowserTestBase,
                               public ::testing::WithParamInterface<TestCase> {
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
    return GetParam().GetGuestMode();
  }

  const char* GetTestCaseName() const override {
    return GetParam().GetTestName();
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
// TODO(noel): get rid of this class, move what it needs into the
// FileManagerBrowserTest class.  Add a |group_name| to TestCase to allow
// detection of tests that need legacy event dispatch by loooking at their
// group name.
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

// INSTANTIATE_TEST_CASE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_CASE_P,
// as WRAPPED_INSTANTIATE_TEST_CASE_P here, so the pre-processor expand macros
// like MAYBE_prefix used to disable tests.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_CASE_P(prefix, test_class, generator)

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FileDisplay,  /* file_display.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("fileDisplayDownloads"),
                      TestCase("fileDisplayDownloads").InGuestMode(),
                      TestCase("fileDisplayDrive"),
                      TestCase("fileDisplayMtp"),
                      TestCase("fileSearch"),
                      TestCase("fileSearchCaseInsensitive"),
                      TestCase("fileSearchNotFound")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenVideoFiles,  /* open_video_files.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive")));

// TIMEOUT PASS on MSAN, https://crbug.com/836254
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenAudioFiles DISABLED_OpenAudioFiles
#else
#define MAYBE_OpenAudioFiles OpenAudioFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenAudioFiles, /* open_audio_files.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("audioOpenDownloads").InGuestMode(),
        TestCase("audioOpenDownloads"),
        TestCase("audioOpenDrive"),
        TestCase("audioAutoAdvanceDrive"),
        TestCase("audioRepeatAllModeSingleFileDrive"),
        TestCase("audioNoRepeatModeSingleFileDrive"),
        TestCase("audioRepeatOneModeSingleFileDrive"),
        TestCase("audioRepeatAllModeMultipleFileDrive"),
        TestCase("audioNoRepeatModeMultipleFileDrive"),
        TestCase("audioRepeatOneModeMultipleFileDrive")));

// Fails on the MSAN bots, https://crbug.com/837551
#if defined(MEMORY_SANITIZER)
#define MAYBE_OpenImageFiles DISABLED_OpenImageFiles
#else
#define MAYBE_OpenImageFiles OpenImageFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenImageFiles, /* open_image_files.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("imageOpenDownloads").InGuestMode(),
                      TestCase("imageOpenDownloads"),
                      TestCase("imageOpenDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    CreateNewFolder, /* create_new_folder.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("selectCreateFolderDownloads"),
        TestCase("createFolderDownloads").InGuestMode(),
        TestCase("createFolderDownloads"),
        TestCase("createFolderDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("keyboardDeleteDownloads").InGuestMode(),
        TestCase("keyboardDeleteDownloads"),
        TestCase("keyboardDeleteDrive"),
        TestCase("keyboardCopyDownloads").InGuestMode(),
        TestCase("keyboardCopyDownloads"),
        TestCase("keyboardCopyDrive"),
        TestCase("renameFileDownloads").InGuestMode(),
        TestCase("renameFileDownloads"),
        TestCase("renameFileDrive"),
        TestCase("renameNewFolderDownloads").InGuestMode(),
        TestCase("renameNewFolderDownloads"),
        TestCase("renameNewFolderDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Delete, /* delete.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("deleteMenuItemNoEntrySelected"),
        TestCase("deleteEntryWithToolbar")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    QuickView, /* quick_view.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("openQuickView"),
                      TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("dirCopyWithContextMenu"),
        TestCase("dirCopyWithContextMenu").InGuestMode(),
        TestCase("dirCopyWithKeyboard"),
        TestCase("dirCopyWithKeyboard").InGuestMode(),
        TestCase("dirCopyWithoutChangingCurrent"),
        TestCase("dirCutWithContextMenu"),
        TestCase("dirCutWithContextMenu").InGuestMode(),
        TestCase("dirCutWithKeyboard"),
        TestCase("dirCutWithKeyboard").InGuestMode(),
        TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirPasteWithContextMenu"),
        TestCase("dirPasteWithContextMenu").InGuestMode(),
        TestCase("dirPasteWithoutChangingCurrent"),
        TestCase("dirRenameWithContextMenu"),
        TestCase("dirRenameWithContextMenu").InGuestMode(),
        TestCase("dirRenameWithKeyboard"),
        TestCase("dirRenameWithKeyboard").InGuestMode(),
        TestCase("dirRenameWithoutChangingCurrent"),
        TestCase("dirRenameToEmptyString"),
        TestCase("dirRenameToEmptyString").InGuestMode(),
        TestCase("dirRenameToExisting"),
        TestCase("dirRenameToExisting").InGuestMode(),
        TestCase("dirCreateWithContextMenu"),
        TestCase("dirCreateWithKeyboard"),
        TestCase("dirCreateWithoutChangingCurrent")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DriveSpecific, /* drive_specific.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("driveOpenSidebarOffline"),
        TestCase("driveOpenSidebarSharedWithMe"),
        TestCase("driveAutoCompleteQuery"),
        TestCase("drivePinFileMobileNetwork"),
        TestCase("driveClickFirstSearchResult"),
        TestCase("drivePressEnterToSearch")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Transfer, /* transfer.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferFromDownloadsToDrive"),
        TestCase("transferFromSharedToDownloads"),
        TestCase("transferFromSharedToDrive"),
        TestCase("transferFromOfflineToDownloads"),
        TestCase("transferFromOfflineToDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    RestorePrefs, /* restore_prefs.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    RestoreGeometry, /* restore_geometry.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("restoreGeometry"),
                      TestCase("restoreGeometry").InGuestMode(),
                      TestCase("restoreGeometryMaximized")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ShareDialog, /* share_dialog.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("shareFile"),
                      TestCase("shareDirectory")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    SuggestAppDialog, /* suggest_app_dialog.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("suggestAppDialog")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Traverse, /* traverse.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ExecuteDefaultTaskOnDownloads, /* tasks.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("executeDefaultTaskDownloads"),
        TestCase("executeDefaultTaskDownloads").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ExecuteDefaultTaskOnDrive, /* tasks.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DefaultTaskDialog, /* tasks.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("defaultTaskDialogDownloads"),
        TestCase("defaultTaskDialogDownloads").InGuestMode(),
        TestCase("defaultTaskDialogDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    GenericTask, /* tasks.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("genericTaskIsNotExecuted"),
        TestCase("genericTaskAndNonGenericTask")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("traverseFolderShortcuts"),
        TestCase("addRemoveFolderShortcuts")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    SortColumns, /* sort_columns.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabIndex, /* tab_index.js */
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(
        TestCase("tabindexSearchBoxFocus")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexFocus, /* tab_index.js */
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestCase("tabindexFocus")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexFocusDownloads, /* tab_index.js */
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestCase("tabindexFocusDownloads"),
                      TestCase("tabindexFocusDownloads").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexFocusDirectorySelected, /* tab_index.js */
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestCase("tabindexFocusDirectorySelected")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexOpenDialog, /* tab_index.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("tabindexOpenDialogDrive"),
        TestCase("tabindexOpenDialogDownloads"),
        TestCase("tabindexOpenDialogDownloads").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabindexSaveFileDialog, /* tab_index.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("tabindexSaveFileDialogDrive"),
        TestCase("tabindexSaveFileDialogDownloads"),
        TestCase("tabindexSaveFileDialogDownloads").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenFileDialog, /* file_dialog.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("openFileDialogDownloads"),
                      TestCase("openFileDialogDownloads").InGuestMode(),
                      TestCase("openFileDialogDrive"),
                      TestCase("openFileDialogDownloads").InIncognito(),
                      TestCase("openFileDialogDrive").InIncognito(),
                      TestCase("openFileDialogUnload")));

// Test does too much? Flaky on all bots: http://crbug.com/500966
WRAPPED_INSTANTIATE_TEST_CASE_P(
    DISABLED_CopyBetweenWindows, /* copy_between_windows.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("copyBetweenWindowsLocalToDrive"),
        TestCase("copyBetweenWindowsLocalToUsb"),
        TestCase("copyBetweenWindowsUsbToDrive"),
        TestCase("copyBetweenWindowsDriveToLocal"),
        TestCase("copyBetweenWindowsDriveToUsb"),
        TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ShowGridView, /* grid_view.js */
    FileManagerBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads"),
                      TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Providers, /* providers.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("requestMount"),
        TestCase("requestMountMultipleMounts"),
        TestCase("requestMountSourceDevice"),
        TestCase("requestMountSourceFile")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    GearMenu, /* gear_menu.js */
    FileManagerBrowserTest,
    ::testing::Values(
        TestCase("showHiddenFilesDownloads"),
        TestCase("showHiddenFilesDrive"),
        TestCase("toogleGoogleDocsDrive"),
        TestCase("showPasteIntoCurrentFolder")));

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

// Flaky crashes in ash::tray::MultiProfileMediaTrayView.
// https://crbug.com/842442
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(LEAK_SANITIZER)
#define MAYBE_PRE_BasicDownloads DISABLED_PRE_BasicDownloads
#define MAYBE_BasicDownloads DISABLED_BasicDownloads
#else
#define MAYBE_PRE_BasicDownloads PRE_BasicDownloads
#define MAYBE_BasicDownloads BasicDownloads
#endif
IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest,
                       MAYBE_PRE_BasicDownloads) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest,
                       MAYBE_BasicDownloads) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDownloads");
  StartTest();
}

// Flaky crashes in ash::tray::MultiProfileMediaTrayView.
// https://crbug.com/842442
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(LEAK_SANITIZER)
#define MAYBE_PRE_BasicDrive DISABLED_PRE_BasicDrive
#define MAYBE_BasicDrive DISABLED_BasicDrive
#else
#define MAYBE_PRE_BasicDrive PRE_BasicDrive
#define MAYBE_BasicDrive BasicDrive
#endif
IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest,
                       MAYBE_PRE_BasicDrive) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFileManagerBrowserTest, MAYBE_BasicDrive) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDrive");
  StartTest();
}

}  // namespace file_manager
