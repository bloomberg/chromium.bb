// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/file_manager/file_manager_browsertest_base.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/chromeos_switches.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace file_manager {

// TestCase: FilesAppBrowserTest parameters.
struct TestCase {
  explicit TestCase(const char* name) : test_case_name(name) {
    CHECK(name) << "FATAL: no test case name";
  }

  TestCase& InGuestMode() {
    guest_mode = IN_GUEST_MODE;
    return *this;
  }

  TestCase& InIncognito() {
    guest_mode = IN_INCOGNITO;
    return *this;
  }

  TestCase& TabletMode() {
    tablet_mode = true;
    return *this;
  }

  TestCase& EnableDriveFs() {
    enable_drivefs = true;
    return *this;
  }

  TestCase& Offline() {
    offline = true;
    return *this;
  }

  // Show the startup browser. Some tests invoke the file picker dialog during
  // the test. Requesting a file picker from a background page is forbidden by
  // the apps platform, and it's a bug that these tests do so.
  // FindRuntimeContext() in select_file_dialog_extension.cc will use the last
  // active browser in this case, which requires a Browser to be present. See
  // https://crbug.com/736930.
  TestCase& WithBrowser() {
    with_browser = true;
    return *this;
  }

  static std::string GetFullTestCaseName(const TestCase& test) {
    std::string name(test.test_case_name);

    CHECK(!name.empty()) << "FATAL: no test case name.";

    if (test.guest_mode == IN_GUEST_MODE)
      name.append("_GuestMode");
    else if (test.guest_mode == IN_INCOGNITO)
      name.append("_Incognito");

    if (test.tablet_mode)
      name.append("_TabletMode");

    if (test.enable_drivefs)
      name.append("_DriveFs");

    return name;
  }

  const char* test_case_name = nullptr;
  GuestMode guest_mode = NOT_IN_GUEST_MODE;
  bool trusted_events = false;
  bool tablet_mode = false;
  bool enable_drivefs = false;
  bool with_browser = false;
  bool needs_zip = false;
  bool offline = false;
};

// EventCase: FilesAppBrowserTest with trusted JS Events.
struct EventCase : public TestCase {
  explicit EventCase(const char* name) : TestCase(name) {
    trusted_events = true;
  }
};

// ZipCase: FilesAppBrowserTest with zip/unzip support.
struct ZipCase : public TestCase {
  explicit ZipCase(const char* name) : TestCase(name) {
    needs_zip = true;
  }
};

// FilesApp browser test.
class FilesAppBrowserTest : public FileManagerBrowserTestBase,
                            public ::testing::WithParamInterface<TestCase> {
 public:
  FilesAppBrowserTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FileManagerBrowserTestBase::SetUpCommandLine(command_line);

    // Prevent Blink swallowing Tab key with its default handlers: forward
    // the Tab key event to the Files.App JS page under test instead.
    if (GetParam().trusted_events) {
      command_line->AppendSwitchASCII("disable-blink-features",
                                      "TrustedEventsDefaultAction");
    }

    // Default mode is clamshell: force Ash into tablet mode if requested.
    if (GetParam().tablet_mode) {
      command_line->AppendSwitchASCII("force-tablet-mode", "touch_view");
    }

    // TODO(crbug.com/879404): Fix tests to work with NativeSMB.
    // Tests assume that no native FSPs are enabled.
    scoped_feature_list_.InitAndDisableFeature(features::kNativeSmb);
  }

  GuestMode GetGuestMode() const override { return GetParam().guest_mode; }

  const char* GetTestCaseName() const override {
    return GetParam().test_case_name;
  }

  std::string GetFullTestCaseName() const override {
    return TestCase::GetFullTestCaseName(GetParam());
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  bool GetEnableDriveFs() const override { return GetParam().enable_drivefs; }

  bool GetRequiresStartupBrowser() const override {
    return GetParam().with_browser;
  }

  bool GetNeedsZipSupport() const override {
    return GetParam().needs_zip;
  }

  bool GetIsOffline() const override { return GetParam().offline; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(FilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_P(FilesAppBrowserTest, Test) {
  StartTest();
}

// INSTANTIATE_TEST_CASE_P expands to code that stringizes the arguments. Thus
// macro parameters such as |prefix| and |test_class| won't be expanded by the
// macro pre-processor. To work around this, indirect INSTANTIATE_TEST_CASE_P,
// as WRAPPED_INSTANTIATE_TEST_CASE_P here, so the pre-processor expands macro
// defines used to disable tests, MAYBE_prefix for example.
#define WRAPPED_INSTANTIATE_TEST_CASE_P(prefix, test_class, generator) \
  INSTANTIATE_TEST_CASE_P(prefix, test_class, generator, &PostTestCaseName)

std::string PostTestCaseName(const ::testing::TestParamInfo<TestCase>& test) {
  return TestCase::GetFullTestCaseName(test.param);
}

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FileDisplay, /* file_display.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("fileDisplayDownloads"),
        TestCase("fileDisplayDownloads").InGuestMode(),
        TestCase("fileDisplayDownloads").TabletMode(),
        TestCase("fileDisplayDrive"),
        TestCase("fileDisplayDrive").TabletMode(),
        TestCase("fileDisplayDrive").EnableDriveFs(),
        TestCase("fileDisplayDriveOffline").Offline().EnableDriveFs(),
        TestCase("fileDisplayDriveOnline").EnableDriveFs(),
        TestCase("fileDisplayDriveOnline"),
        TestCase("fileDisplayMtp"),
        TestCase("fileDisplayUsb"),
        TestCase("fileSearch"),
        TestCase("fileSearchCaseInsensitive"),
        TestCase("fileSearchNotFound")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenVideoFiles, /* open_video_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("videoOpenDownloads").InGuestMode(),
                      TestCase("videoOpenDownloads"),
                      TestCase("videoOpenDrive"),
                      TestCase("videoOpenDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenAudioFiles, /* open_audio_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("audioOpenCloseDownloads"),
                      TestCase("audioOpenCloseDownloads").InGuestMode(),
                      TestCase("audioOpenCloseDrive"),
                      TestCase("audioOpenDownloads").InGuestMode(),
                      TestCase("audioOpenDownloads"),
                      TestCase("audioOpenDrive"),
                      TestCase("audioOpenDrive").EnableDriveFs(),
                      TestCase("audioAutoAdvanceDrive"),
                      TestCase("audioRepeatAllModeSingleFileDrive"),
                      TestCase("audioNoRepeatModeSingleFileDrive"),
                      TestCase("audioRepeatOneModeSingleFileDrive"),
                      TestCase("audioRepeatAllModeMultipleFileDrive"),
                      TestCase("audioNoRepeatModeMultipleFileDrive"),
                      TestCase("audioRepeatOneModeMultipleFileDrive")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    OpenImageFiles, /* open_image_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("imageOpenDownloads").InGuestMode(),
                      TestCase("imageOpenDownloads"),
                      TestCase("imageOpenDrive"),
                      TestCase("imageOpenDrive").EnableDriveFs(),
                      TestCase("imageOpenGalleryOpenDownloads"),
                      TestCase("imageOpenGalleryOpenDrive"),
                      TestCase("imageOpenGalleryOpenDrive").EnableDriveFs()));

// NaCl fails to compile zip plugin.pexe too often on ASAN, crbug.com/867738
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ZipFiles DISABLED_ZipFiles
#else
#define MAYBE_ZipFiles ZipFiles
#endif
WRAPPED_INSTANTIATE_TEST_CASE_P(
    MAYBE_ZipFiles, /* zip_files.js */
    FilesAppBrowserTest,
    ::testing::Values(ZipCase("zipFileOpenDownloads").InGuestMode(),
                      ZipCase("zipFileOpenDownloads"),
                      ZipCase("zipFileOpenDrive").EnableDriveFs(),
                      ZipCase("zipFileOpenDrive"),
                      ZipCase("zipFileOpenUsb"),
                      ZipCase("zipCreateFileDownloads").InGuestMode(),
                      ZipCase("zipCreateFileDownloads"),
                      ZipCase("zipCreateFileDrive").EnableDriveFs(),
                      ZipCase("zipCreateFileDrive"),
                      ZipCase("zipCreateFileUsb")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    CreateNewFolder, /* create_new_folder.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("selectCreateFolderDownloads").InGuestMode(),
                      TestCase("selectCreateFolderDownloads"),
                      TestCase("createFolderDownloads").InGuestMode(),
                      TestCase("createFolderDownloads"),
                      TestCase("createFolderNestedDownloads"),
                      TestCase("createFolderDrive"),
                      TestCase("createFolderDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    KeyboardOperations, /* keyboard_operations.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("keyboardDeleteDownloads").InGuestMode(),
                      TestCase("keyboardDeleteDownloads"),
                      TestCase("keyboardDeleteDrive"),
                      TestCase("keyboardDeleteDrive").EnableDriveFs(),
                      TestCase("keyboardDeleteFolderDownloads").InGuestMode(),
                      TestCase("keyboardDeleteFolderDownloads"),
                      TestCase("keyboardDeleteFolderDrive"),
                      TestCase("keyboardDeleteFolderDrive").EnableDriveFs(),
                      TestCase("keyboardCopyDownloads").InGuestMode(),
                      TestCase("keyboardCopyDownloads"),
                      TestCase("keyboardCopyDrive"),
                      TestCase("keyboardCopyDrive").EnableDriveFs(),
                      TestCase("renameFileDownloads").InGuestMode(),
                      TestCase("renameFileDownloads"),
                      TestCase("renameFileDrive"),
                      TestCase("renameFileDrive").EnableDriveFs(),
                      TestCase("renameNewFolderDownloads").InGuestMode(),
                      TestCase("renameNewFolderDownloads"),
                      TestCase("renameNewFolderDrive"),
                      TestCase("renameNewFolderDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ContextMenu, /* context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("checkDeleteEnabledForReadWriteFile"),
                      TestCase("checkDeleteDisabledForReadOnlyDocument"),
                      TestCase("checkDeleteDisabledForReadOnlyFile"),
                      TestCase("checkDeleteDisabledForReadOnlyFolder"),
                      TestCase("checkRenameEnabledForReadWriteFile"),
                      TestCase("checkRenameDisabledForReadOnlyDocument"),
                      TestCase("checkRenameDisabledForReadOnlyFile"),
                      TestCase("checkRenameDisabledForReadOnlyFolder"),
                      TestCase("checkShareEnabledForReadWriteFile"),
                      TestCase("checkShareEnabledForReadOnlyDocument"),
                      TestCase("checkShareDisabledForStrictReadOnlyDocument"),
                      TestCase("checkShareEnabledForReadOnlyFile"),
                      TestCase("checkShareEnabledForReadOnlyFolder"),
                      TestCase("checkCopyEnabledForReadWriteFile"),
                      TestCase("checkCopyEnabledForReadOnlyDocument"),
                      TestCase("checkCopyDisabledForStrictReadOnlyDocument"),
                      TestCase("checkCopyEnabledForReadOnlyFile"),
                      TestCase("checkCopyEnabledForReadOnlyFolder"),
                      TestCase("checkCutEnabledForReadWriteFile"),
                      TestCase("checkCutDisabledForReadOnlyDocument"),
                      TestCase("checkCutDisabledForReadOnlyFile"),
                      TestCase("checkCutDisabledForReadOnlyFolder"),
                      TestCase("checkPasteIntoFolderEnabledForReadWriteFolder"),
                      TestCase("checkPasteIntoFolderDisabledForReadOnlyFolder"),
                      TestCase("checkContextMenusForInputElements"),
                      TestCase("checkNewFolderEnabledInsideReadWriteFolder"),
                      TestCase("checkNewFolderDisabledInsideReadOnlyFolder"),
                      TestCase("checkPasteEnabledInsideReadWriteFolder"),
                      TestCase("checkPasteDisabledInsideReadOnlyFolder"),
                      TestCase("checkCopyEnabledForReadWriteFolderInTree"),
                      TestCase("checkCopyEnabledForReadOnlyFolderInTree"),
                      TestCase("checkCutEnabledForReadWriteFolderInTree"),
                      TestCase("checkCutDisabledForReadOnlyFolderInTree"),
                      TestCase("checkPasteEnabledForReadWriteFolderInTree"),
                      TestCase("checkPasteDisabledForReadOnlyFolderInTree"),
                      TestCase("checkContextMenuForTeamDriveRoot")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Delete, /* delete.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("deleteMenuItemNoEntrySelected"),
                      TestCase("deleteEntryWithToolbar").InGuestMode(),
                      TestCase("deleteEntryWithToolbar")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    QuickView, /* quick_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("openQuickView"),
                      TestCase("openQuickView").InGuestMode(),
                      TestCase("openQuickView").TabletMode(),
                      TestCase("openQuickViewDrive"),
                      TestCase("openQuickViewDrive").EnableDriveFs(),
                      TestCase("openQuickViewUsb"),
                      TestCase("openQuickViewMtp"),
                      TestCase("closeQuickView")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    DirectoryTreeContextMenu, /* directory_tree_context_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("dirCopyWithContextMenu"),
                      TestCase("dirCopyWithContextMenu").InGuestMode(),
                      TestCase("dirCopyWithKeyboard"),
                      TestCase("dirCopyWithKeyboard").InGuestMode(),
                      TestCase("dirCopyWithoutChangingCurrent"),
                      TestCase("dirCutWithContextMenu"),
                      TestCase("dirCutWithContextMenu").InGuestMode(),
                      TestCase("dirCutWithKeyboard"),
                      TestCase("dirCutWithKeyboard").InGuestMode(),
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
    FilesAppBrowserTest,
    ::testing::Values(TestCase("driveOpenSidebarOffline"),
                      TestCase("driveOpenSidebarSharedWithMe"),
                      TestCase("driveAutoCompleteQuery"),
                      TestCase("drivePinFileMobileNetwork"),
                      TestCase("drivePinFileMobileNetwork").EnableDriveFs(),
                      TestCase("driveClickFirstSearchResult"),
                      TestCase("drivePressEnterToSearch"),
                      TestCase("drivePressCtrlAFromSearch")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Transfer, /* transfer.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("transferFromDriveToDownloads"),
        TestCase("transferFromDriveToDownloads").EnableDriveFs(),
        TestCase("transferFromDownloadsToDrive"),
        TestCase("transferFromDownloadsToDrive").EnableDriveFs(),
        TestCase("transferFromSharedToDownloads"),
        TestCase("transferFromSharedToDrive"),
        TestCase("transferFromOfflineToDownloads"),
        TestCase("transferFromOfflineToDrive"),
        TestCase("transferFromTeamDriveToDrive"),
        TestCase("transferFromTeamDriveToDrive").EnableDriveFs(),
        TestCase("transferFromDriveToTeamDrive"),
        TestCase("transferFromDriveToTeamDrive").EnableDriveFs(),
        TestCase("transferFromTeamDriveToDownloads"),
        TestCase("transferFromTeamDriveToDownloads").EnableDriveFs(),
        TestCase("transferHostedFileFromTeamDriveToDownloads"),
        TestCase("transferHostedFileFromTeamDriveToDownloads").EnableDriveFs(),
        TestCase("transferFromDownloadsToTeamDrive"),
        TestCase("transferFromDownloadsToTeamDrive").EnableDriveFs(),
        TestCase("transferBetweenTeamDrives"),
        TestCase("transferBetweenTeamDrives").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    RestorePrefs, /* restore_prefs.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreSortColumn").InGuestMode(),
                      TestCase("restoreSortColumn"),
                      TestCase("restoreCurrentView").InGuestMode(),
                      TestCase("restoreCurrentView")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    RestoreGeometry, /* restore_geometry.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("restoreGeometry"),
                      TestCase("restoreGeometry").InGuestMode(),
                      TestCase("restoreGeometryMaximized")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    ShareAndManageDialog, /* share_and_manage_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("shareFileDrive"),
                      TestCase("shareDirectoryDrive"),
                      TestCase("manageHostedFileDrive"),
                      TestCase("manageHostedFileDrive").EnableDriveFs(),
                      TestCase("manageFileDrive"),
                      TestCase("manageFileDrive").EnableDriveFs(),
                      TestCase("manageDirectoryDrive"),
                      TestCase("manageDirectoryDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    SuggestAppDialog, /* suggest_app_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("suggestAppDialog")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Traverse, /* traverse.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseDownloads").InGuestMode(),
                      TestCase("traverseDownloads"),
                      TestCase("traverseDrive"),
                      TestCase("traverseDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Tasks, /* tasks.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("executeDefaultTaskDownloads"),
                      TestCase("executeDefaultTaskDownloads").InGuestMode(),
                      TestCase("executeDefaultTaskDrive"),
                      TestCase("executeDefaultTaskDrive").EnableDriveFs(),
                      TestCase("defaultTaskDialogDownloads"),
                      TestCase("defaultTaskDialogDownloads").InGuestMode(),
                      TestCase("defaultTaskDialogDrive"),
                      TestCase("defaultTaskDialogDrive").EnableDriveFs(),
                      TestCase("genericTaskIsNotExecuted"),
                      TestCase("genericTaskAndNonGenericTask")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FolderShortcuts, /* folder_shortcuts.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("traverseFolderShortcuts"),
                      TestCase("addRemoveFolderShortcuts")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    SortColumns, /* sort_columns.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("sortColumns"),
                      TestCase("sortColumns").InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    TabIndex, /* tab_index.js: tabindex tests require trusted JS Events. */
    FilesAppBrowserTest,
    ::testing::Values(
        EventCase("tabindexSearchBoxFocus"),
        EventCase("tabindexFocus"),
        EventCase("tabindexFocusDownloads"),
        EventCase("tabindexFocusDownloads").InGuestMode(),
        EventCase("tabindexFocusDirectorySelected"),
        EventCase("tabindexOpenDialogDrive").WithBrowser(),
        EventCase("tabindexOpenDialogDrive").WithBrowser().EnableDriveFs(),
        EventCase("tabindexOpenDialogDownloads").WithBrowser(),
        EventCase("tabindexOpenDialogDownloads").WithBrowser().InGuestMode(),
        EventCase("tabindexSaveFileDialogDrive").WithBrowser(),
        EventCase("tabindexSaveFileDialogDrive").WithBrowser().EnableDriveFs(),
        EventCase("tabindexSaveFileDialogDownloads").WithBrowser(),
        EventCase("tabindexSaveFileDialogDownloads")
            .WithBrowser()
            .InGuestMode()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    FileDialog, /* file_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("openFileDialogUnload").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser(),
        TestCase("openFileDialogDownloads").WithBrowser().InGuestMode(),
        TestCase("openFileDialogDownloads").WithBrowser().InIncognito(),
        TestCase("openFileDialogCancelDownloads").WithBrowser(),
        TestCase("openFileDialogEscapeDownloads").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser(),
        TestCase("openFileDialogDrive").WithBrowser().InIncognito(),
        TestCase("openFileDialogDrive").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogDrive")
            .WithBrowser()
            .InIncognito()
            .EnableDriveFs(),
        TestCase("openFileDialogCancelDrive").WithBrowser(),
        TestCase("openFileDialogCancelDrive").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogEscapeDrive").WithBrowser(),
        TestCase("openFileDialogEscapeDrive").WithBrowser().EnableDriveFs(),
        TestCase("openFileDialogDriveOffline")
            .WithBrowser()
            .Offline()
            .EnableDriveFs(),
        TestCase("openFileDialogDriveOfflinePinned")
            .WithBrowser()
            .Offline()
            .EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    CopyBetweenWindows, /* copy_between_windows.js */
    FilesAppBrowserTest,
    ::testing::Values(
        TestCase("copyBetweenWindowsLocalToDrive"),
        TestCase("copyBetweenWindowsLocalToDrive").EnableDriveFs(),
        TestCase("copyBetweenWindowsLocalToUsb"),
        TestCase("copyBetweenWindowsUsbToDrive"),
        TestCase("copyBetweenWindowsUsbToDrive").EnableDriveFs(),
        TestCase("copyBetweenWindowsDriveToLocal"),
        TestCase("copyBetweenWindowsDriveToLocal").EnableDriveFs(),
        TestCase("copyBetweenWindowsDriveToUsb"),
        TestCase("copyBetweenWindowsDriveToUsb").EnableDriveFs(),
        TestCase("copyBetweenWindowsUsbToLocal")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    GridView, /* grid_view.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showGridViewDownloads"),
                      TestCase("showGridViewDownloads").InGuestMode(),
                      TestCase("showGridViewDrive"),
                      TestCase("showGridViewDrive").EnableDriveFs()));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Providers, /* providers.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("requestMount"),
                      TestCase("requestMountMultipleMounts"),
                      TestCase("requestMountSourceDevice"),
                      TestCase("requestMountSourceFile")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    GearMenu, /* gear_menu.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showHiddenFilesDownloads"),
                      TestCase("showHiddenFilesDownloads").InGuestMode(),
                      TestCase("showHiddenFilesDrive"),
                      TestCase("showHiddenFilesDrive").EnableDriveFs(),
                      TestCase("toogleGoogleDocsDrive"),
                      TestCase("toogleGoogleDocsDrive").EnableDriveFs(),
                      TestCase("showPasteIntoCurrentFolder"),
                      TestCase("showSelectAllInCurrentFolder")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    Crostini, /* crostini.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("mountCrostiniContainer")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    MyFiles, /* my_files.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("showMyFiles"),
                      TestCase("hideSearchButton"),
                      TestCase("myFilesDisplaysAndOpensEntries"),
                      TestCase("directoryTreeRefresh"),
                      TestCase("myFilesUpdatesChildren")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    InstallLinuxPackageDialog, /* install_linux_package_dialog.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("installLinuxPackageDialog")));

WRAPPED_INSTANTIATE_TEST_CASE_P(
    LauncherSearch, /* launcher_search.js */
    FilesAppBrowserTest,
    ::testing::Values(TestCase("launcherOpenSearchResult")));

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
class MultiProfileFilesAppBrowserTest : public FileManagerBrowserTestBase {
 public:
  MultiProfileFilesAppBrowserTest() = default;

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

  std::string GetFullTestCaseName() const override {
    return test_case_name_;
  }

  const char* GetTestExtensionManifestName() const override {
    return "file_manager_test_manifest.json";
  }

  void set_test_case_name(const std::string& name) { test_case_name_ = name; }

 private:
  std::string test_case_name_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileFilesAppBrowserTest);
};

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, PRE_BasicDownloads) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, BasicDownloads) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDownloads");
  StartTest();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, PRE_BasicDrive) {
  AddAllUsers();
}

IN_PROC_BROWSER_TEST_F(MultiProfileFilesAppBrowserTest, BasicDrive) {
  AddAllUsers();
  // Sanity check that normal operations work in multi-profile.
  set_test_case_name("keyboardCopyDrive");
  StartTest();
}

}  // namespace file_manager
