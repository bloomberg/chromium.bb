// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chromeos/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"

namespace file_manager {

// Parameter of FileManagerBrowserTest.
// The second value is the case name of JavaScript.
typedef std::tr1::tuple<GuestMode, const char*> TestParameter;

// Test fixture class for normal (not multi-profile related) tests.
class FileManagerBrowserTest :
      public FileManagerBrowserTestBase,
      public ::testing::WithParamInterface<TestParameter> {
  GuestMode GetGuestModeParam() const override {
    return std::tr1::get<0>(GetParam());
  }
  const char* GetTestManifestName() const override {
    return "file_manager_test_manifest.json";
  }
  const char* GetTestCaseNameParam() const override {
    return std::tr1::get<1>(GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(FileManagerBrowserTest, Test) {
  StartTest();
}

// Test fixture class for tests that rely on deprecated event dispatch that send
// tests.
class FileManagerBrowserTestWithLegacyEventDispatch
    : public FileManagerBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("disable-blink-features",
                                    "TrustedEventsDefaultAction");
  }
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

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_FileDisplay DISABLED_FileDisplay
#else
#define MAYBE_FileDisplay FileDisplay
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_FileDisplay,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "fileDisplayDownloads"),
                      TestParameter(IN_GUEST_MODE, "fileDisplayDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileDisplayDrive"),
                      TestParameter(NOT_IN_GUEST_MODE, "fileDisplayMtp"),
                      TestParameter(NOT_IN_GUEST_MODE, "searchNormal"),
                      TestParameter(NOT_IN_GUEST_MODE, "searchCaseInsensitive"),
                      TestParameter(NOT_IN_GUEST_MODE, "searchNotFound")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenVideoFiles DISABLED_OpenVideoFiles
#else
#define MAYBE_OpenVideoFiles OpenVideoFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenVideoFiles,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "videoOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "videoOpenDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "videoOpenDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenAudioFiles DISABLED_OpenAudioFiles
#else
#define MAYBE_OpenAudioFiles OpenAudioFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenAudioFiles,
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

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
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

#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_CreateNewFolder DISABLED_CreateNewFolder
#else
#define MAYBE_CreateNewFolder CreateNewFolder
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_CreateNewFolder,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "createNewFolderAfterSelectFile"),
                      TestParameter(IN_GUEST_MODE,
                                    "createNewFolderDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "createNewFolderDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "createNewFolderDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_KeyboardOperations DISABLED_KeyboardOperations
#else
#define MAYBE_KeyboardOperations KeyboardOperations
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_KeyboardOperations,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "keyboardDeleteDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "keyboardDeleteDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "keyboardDeleteDrive"),
                      TestParameter(IN_GUEST_MODE, "keyboardCopyDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "keyboardCopyDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "keyboardCopyDrive"),
                      TestParameter(IN_GUEST_MODE, "renameFileDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "renameFileDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "renameFileDrive"),
                      TestParameter(IN_GUEST_MODE,
                                    "renameNewDirectoryDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "renameNewDirectoryDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "renameNewDirectoryDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_Delete DISABLED_Delete
#else
#define MAYBE_Delete Delete
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Delete,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE,
                      "deleteMenuItemIsDisabledWhenNoItemIsSelected"),
        TestParameter(NOT_IN_GUEST_MODE, "deleteOneItemFromToolbar")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DISABLED_QuickView,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "openQuickView")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DirectoryTreeContextMenu DISABLED_DirectoryTreeContextMenu
#else
#define MAYBE_DirectoryTreeContextMenu DirectoryTreeContextMenu
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_DirectoryTreeContextMenu,
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

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_DriveSpecific DISABLED_DriveSpecific
#else
#define MAYBE_DriveSpecific DriveSpecific
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_DriveSpecific,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarRecent"),
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarOffline"),
        TestParameter(NOT_IN_GUEST_MODE, "openSidebarSharedWithMe"),
        TestParameter(NOT_IN_GUEST_MODE, "autocomplete"),
        TestParameter(NOT_IN_GUEST_MODE, "pinFileOnMobileNetwork"),
        TestParameter(NOT_IN_GUEST_MODE, "clickFirstSearchResult"),
        TestParameter(NOT_IN_GUEST_MODE, "pressEnterToSearch")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_Transfer DISABLED_Transfer
#else
#define MAYBE_Transfer Transfer
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Transfer,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "transferFromDriveToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromDownloadsToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromSharedToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromSharedToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromRecentToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromRecentToDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromOfflineToDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "transferFromOfflineToDrive")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_RestorePrefs DISABLED_RestorePrefs
#else
#define MAYBE_RestorePrefs RestorePrefs
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_RestorePrefs,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "restoreSortColumn"),
                      TestParameter(NOT_IN_GUEST_MODE, "restoreSortColumn"),
                      TestParameter(IN_GUEST_MODE, "restoreCurrentView"),
                      TestParameter(NOT_IN_GUEST_MODE, "restoreCurrentView")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ShareDialog DISABLED_ShareDialog
#else
#define MAYBE_ShareDialog ShareDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ShareDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "shareFile"),
                      TestParameter(NOT_IN_GUEST_MODE, "shareDirectory")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_RestoreGeometry DISABLED_RestoreGeometry
#else
#define MAYBE_RestoreGeometry RestoreGeometry
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_RestoreGeometry,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "restoreGeometry"),
                      TestParameter(IN_GUEST_MODE, "restoreGeometry"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "restoreGeometryMaximizedState")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_Traverse DISABLED_Traverse
#else
#define MAYBE_Traverse Traverse
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Traverse,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(IN_GUEST_MODE, "traverseDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "traverseDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "traverseDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_SuggestAppDialog DISABLED_SuggestAppDialog
#else
#define MAYBE_SuggestAppDialog SuggestAppDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_SuggestAppDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "suggestAppDialog")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExecuteDefaultTaskOnDownloads \
  DISABLED_ExecuteDefaultTaskOnDownloads
#else
#define MAYBE_ExecuteDefaultTaskOnDownloads ExecuteDefaultTaskOnDownloads
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ExecuteDefaultTaskOnDownloads,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "executeDefaultTaskOnDownloads"),
        TestParameter(IN_GUEST_MODE, "executeDefaultTaskOnDownloads")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ExecuteDefaultTaskOnDrive DISABLED_ExecuteDefaultTaskOnDrive
#else
#define MAYBE_ExecuteDefaultTaskOnDrive ExecuteDefaultTaskOnDrive
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ExecuteDefaultTaskOnDrive,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "executeDefaultTaskOnDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_DefaultTaskDialog DISABLED_DefaultTaskDialog
#else
#define MAYBE_DefaultTaskDialog DefaultTaskDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_DefaultTaskDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "defaultTaskDialogOnDownloads"),
        TestParameter(IN_GUEST_MODE, "defaultTaskDialogOnDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "defaultTaskDialogOnDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_GenericTask DISABLED_GenericTask
#else
#define MAYBE_GenericTask GenericTask
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_GenericTask,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "genericTaskIsNotExecuted"),
        TestParameter(NOT_IN_GUEST_MODE, "genericAndNonGenericTasksAreMixed")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_FolderShortcuts DISABLED_FolderShortcuts
#else
#define MAYBE_FolderShortcuts FolderShortcuts
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_FolderShortcuts,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "traverseFolderShortcuts"),
        TestParameter(NOT_IN_GUEST_MODE, "addRemoveFolderShortcuts")));

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_SortColumns DISABLED_SortColumns
#else
#define MAYBE_SortColumns SortColumns
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_SortColumns,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "sortColumns"),
                      TestParameter(IN_GUEST_MODE, "sortColumns")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_TabIndex DISABLED_TabIndex
#else
#define MAYBE_TabIndex TabIndex
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabIndex,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "searchBoxFocus")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_TabindexFocus DISABLED_TabindexFocus
#else
#define MAYBE_TabindexFocus TabindexFocus
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabindexFocus,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "tabindexFocus")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_TabindexFocusDownloads DISABLED_TabindexFocusDownloads
#else
#define MAYBE_TabindexFocusDownloads TabindexFocusDownloads
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabindexFocusDownloads,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "tabindexFocusDownloads"),
                      TestParameter(IN_GUEST_MODE, "tabindexFocusDownloads")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_TabindexFocusDirectorySelected \
  DISABLED_TabindexFocusDirectorySelected
#else
#define MAYBE_TabindexFocusDirectorySelected TabindexFocusDirectorySelected
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_TabindexFocusDirectorySelected,
    FileManagerBrowserTestWithLegacyEventDispatch,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "tabindexFocusDirectorySelected")));

// Fails on official cros trunk build. http://crbug.com/480491
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_TabindexOpenDialog DISABLED_TabindexOpenDialog
#else
#define MAYBE_TabindexOpenDialog TabindexOpenDialog
#endif
// Flaky: crbug.com/615259
WRAPPED_INSTANTIATE_TEST_CASE_P(
    DISABLED_TabindexOpenDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexOpenDialogDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "tabindexOpenDialogDownloads"),
        TestParameter(IN_GUEST_MODE, "tabindexOpenDialogDownloads")));

// Fails on official build. http://crbug.com/482121.
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_TabindexSaveFileDialog DISABLED_TabindexSaveFileDialog
#else
#define MAYBE_TabindexSaveFileDialog TabindexSaveFileDialog
#endif
// Flaky: crbug.com/615259
WRAPPED_INSTANTIATE_TEST_CASE_P(
    DISABLED_TabindexSaveFileDialog,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSaveFileDialogDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "tabindexSaveFileDialogDownloads"),
        TestParameter(IN_GUEST_MODE, "tabindexSaveFileDialogDownloads")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
#define MAYBE_OpenFileDialog DISABLED_OpenFileDialog
#else
#define MAYBE_OpenFileDialog OpenFileDialog
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_OpenFileDialog,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE,
                                    "openFileDialogOnDownloads"),
                      TestParameter(IN_GUEST_MODE,
                                    "openFileDialogOnDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "openFileDialogOnDrive"),
                      TestParameter(IN_INCOGNITO,
                                    "openFileDialogOnDownloads"),
                      TestParameter(IN_INCOGNITO,
                                    "openFileDialogOnDrive"),
                      TestParameter(NOT_IN_GUEST_MODE,
                                    "unloadFileDialog")));

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

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_ShowGridView DISABLED_ShowGridView
#else
#define MAYBE_ShowGridView ShowGridView
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ShowGridView,
    FileManagerBrowserTest,
    ::testing::Values(TestParameter(NOT_IN_GUEST_MODE, "showGridViewDownloads"),
                      TestParameter(IN_GUEST_MODE, "showGridViewDownloads"),
                      TestParameter(NOT_IN_GUEST_MODE, "showGridViewDrive")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_Providers DISABLED_Providers
#else
#define MAYBE_Providers Providers
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_Providers,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "requestMount"),
        TestParameter(NOT_IN_GUEST_MODE, "requestMountMultipleMounts"),
        TestParameter(NOT_IN_GUEST_MODE, "requestMountSourceDevice"),
        TestParameter(NOT_IN_GUEST_MODE, "requestMountSourceFile")));

#if defined(DISABLE_SLOW_FILESAPP_TESTS)
#define MAYBE_GearMenu DISABLED_GearMenu
#else
#define MAYBE_GearMenu GearMenu
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_GearMenu,
    FileManagerBrowserTest,
    ::testing::Values(
        TestParameter(NOT_IN_GUEST_MODE, "showHiddenFilesOnDownloads"),
        TestParameter(NOT_IN_GUEST_MODE, "showHiddenFilesOnDrive"),
        TestParameter(NOT_IN_GUEST_MODE, "hideGoogleDocs")));

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
    for (size_t i = 0; i < arraysize(kTestAccounts); ++i)
      AddUser(kTestAccounts[i], i >= SECONDARY_ACCOUNT_INDEX_START);
  }

  // Returns primary profile (if it is already created.)
  Profile* profile() override {
    Profile* const profile = chromeos::ProfileHelper::GetProfileByUserIdHash(
        kTestAccounts[PRIMARY_ACCOUNT_INDEX].hash);
    return profile ? profile : FileManagerBrowserTestBase::profile();
  }

  // Sets the test case name (used as a function name in test_cases.js to call.)
  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

  // Adds a new user for testing to the current session.
  void AddUser(const TestAccountInfo& info, bool log_in) {
    const AccountId account_id(AccountId::FromUserEmail(info.email));
    if (log_in) {
      session_manager::SessionManager::Get()->CreateSession(account_id,
                                                            info.hash);
    }
    user_manager::UserManager::Get()->SaveUserDisplayName(
        account_id, base::UTF8ToUTF16(info.display_name));
    SigninManagerFactory::GetForProfile(
        chromeos::ProfileHelper::GetProfileByUserIdHash(info.hash))
        ->SetAuthenticatedAccountInfo(info.gaia_id, info.email);
  }

 private:
  GuestMode GetGuestModeParam() const override { return NOT_IN_GUEST_MODE; }
  const char* GetTestManifestName() const override {
    return "file_manager_test_manifest.json";
  }
  const char* GetTestCaseNameParam() const override {
    return test_case_name_.c_str();
  }

  std::string test_case_name_;
};

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
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

  // Sanity check that normal operations work in multi-profile setting as well.
  set_test_case_name("keyboardCopyDownloads");
  StartTest();
}

// Fails on official build. http://crbug.com/429294
#if defined(DISABLE_SLOW_FILESAPP_TESTS) || defined(OFFICIAL_BUILD)
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

  // Sanity check that normal operations work in multi-profile setting as well.
  set_test_case_name("keyboardCopyDrive");
  StartTest();
}

}  // namespace file_manager
