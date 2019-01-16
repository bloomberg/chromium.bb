// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_mac.h"

#import <Cocoa/Cocoa.h>
#include <libproc.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_manager_mac.h"
#include "chrome/browser/browser_process.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/mac/install_from_dmg.h"
#import "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/mac/mac_startup_profiler.h"
#include "chrome/browser/ui/cocoa/main_menu_builder.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "components/crash/content/app/crashpad.h"
#include "components/metrics/metrics_service.h"
#include "components/os_crypt/os_crypt.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/result_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_handle.h"
#include "ui/native_theme/native_theme_mac.h"

namespace {

// Writes an undocumented sentinel file that prevents Spotlight from indexing
// below a particular path in order to reap some power savings.
void EnsureMetadataNeverIndexFileOnFileThread(
    const base::FilePath& user_data_dir) {
  const char kMetadataNeverIndexFilename[] = ".metadata_never_index";
  base::FilePath metadata_file_path =
      user_data_dir.Append(kMetadataNeverIndexFilename);
  if (base::PathExists(metadata_file_path))
    return;

  if (base::WriteFile(metadata_file_path, nullptr, 0) == -1)
    DLOG(FATAL) << "Could not write .metadata_never_index file.";
}

void EnsureMetadataNeverIndexFile(const base::FilePath& user_data_dir) {
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
      base::Bind(&EnsureMetadataNeverIndexFileOnFileThread, user_data_dir));
}

// Used for UMA; never alter existing values.
enum class FilesystemType {
  kUnknown,
  kOther,
  k_acfs,
  k_afpfs,
  k_apfs,
  k_cdd9660,
  k_cddafs,
  k_exfat,
  k_ftp,
  k_hfs,
  k_hfs_rodmg,
  k_msdos,
  k_nfs,
  k_ntfs,
  k_smbfs,
  k_udf,
  k_webdav,
  kGoogleDriveFS,
  kMaxValue = kGoogleDriveFS,
};

FilesystemType FilesystemStringToType(DiskImageStatus is_ro_dmg,
                                      NSString* filesystem_type) {
  if ([filesystem_type isEqualToString:@"acfs"])
    return FilesystemType::k_acfs;
  if ([filesystem_type isEqualToString:@"afpfs"])
    return FilesystemType::k_afpfs;
  if ([filesystem_type isEqualToString:@"apfs"])
    return FilesystemType::k_apfs;
  if ([filesystem_type isEqualToString:@"cdd9660"])
    return FilesystemType::k_cdd9660;
  if ([filesystem_type isEqualToString:@"cddafs"])
    return FilesystemType::k_cddafs;
  if ([filesystem_type isEqualToString:@"exfat"])
    return FilesystemType::k_exfat;
  if ([filesystem_type isEqualToString:@"hfs"]) {
    switch (is_ro_dmg) {
      case DiskImageStatusFailure:
      case DiskImageStatusFalse:
        return FilesystemType::k_hfs;
        break;

      case DiskImageStatusTrue:
        return FilesystemType::k_hfs_rodmg;
        break;
    }
  }
  if ([filesystem_type isEqualToString:@"msdos"])
    return FilesystemType::k_msdos;
  if ([filesystem_type isEqualToString:@"nfs"])
    return FilesystemType::k_nfs;
  if ([filesystem_type isEqualToString:@"ntfs"])
    return FilesystemType::k_ntfs;
  if ([filesystem_type isEqualToString:@"smbfs"])
    return FilesystemType::k_smbfs;
  if ([filesystem_type isEqualToString:@"udf"])
    return FilesystemType::k_udf;
  if ([filesystem_type isEqualToString:@"webdav"])
    return FilesystemType::k_webdav;
  if ([filesystem_type isEqualToString:@"dfsfuse_DFS"])
    return FilesystemType::kGoogleDriveFS;
  return FilesystemType::kOther;
}

void RecordFilesystemStats() {
  DiskImageStatus is_ro_dmg = IsAppRunningFromReadOnlyDiskImage(nullptr);
  // Note that -getFileSystemInfoForPath:... is implemented with Disk
  // Arbitration and |filesystem_type_string| is the value from
  // kDADiskDescriptionVolumeKindKey. Furthermore, for built-in filesystems, the
  // string returned specifies which file in /System/Library/Filesystems is
  // handling it.
  NSString* filesystem_type_string;
  BOOL success = [[NSWorkspace sharedWorkspace]
      getFileSystemInfoForPath:[base::mac::OuterBundle() bundlePath]
                   isRemovable:nil
                    isWritable:nil
                 isUnmountable:nil
                   description:nil
                          type:&filesystem_type_string];

  FilesystemType filesystem_type = FilesystemType::kUnknown;
  if (success)
    filesystem_type = FilesystemStringToType(is_ro_dmg, filesystem_type_string);

  UMA_HISTOGRAM_ENUMERATION("OSX.InstallationFilesystem", filesystem_type);
}

// Get the uid and executable path for a pid. Returns true iff successful.
// |path_buffer| must be of PROC_PIDPATHINFO_MAXSIZE length.
bool GetUIDAndPathOfPID(pid_t pid, char* path_buffer, uid_t* out_uid) {
  struct proc_bsdshortinfo info;
  int error = proc_pidinfo(pid, PROC_PIDT_SHORTBSDINFO, 0, &info, sizeof(info));
  if (error <= 0)
    return false;

  error = proc_pidpath(pid, path_buffer, PROC_PIDPATHINFO_MAXSIZE);
  if (error <= 0)
    return false;

  *out_uid = info.pbsi_uid;
  return true;
}

void RecordInstanceStats() {
  // Get list of all processes.

  int pid_array_size_needed = proc_listallpids(nullptr, 0);
  if (pid_array_size_needed <= 0)
    return;
  std::vector<pid_t> pid_array(pid_array_size_needed * 4);  // slack
  int pid_count = proc_listallpids(pid_array.data(),
                                   pid_array.size() * sizeof(pid_array[0]));
  if (pid_count <= 0)
    return;

  pid_array.resize(pid_count);

  // Get info about this process.

  const pid_t this_pid = getpid();
  uid_t this_uid;
  char this_path[PROC_PIDPATHINFO_MAXSIZE];
  if (!GetUIDAndPathOfPID(this_pid, this_path, &this_uid))
    return;

  // Compare all other processes to this one.

  int this_user_count = 0;
  int other_user_count = 0;
  for (pid_t pid : pid_array) {
    if (pid == this_pid)
      continue;

    uid_t uid;
    char path[PROC_PIDPATHINFO_MAXSIZE];
    if (!GetUIDAndPathOfPID(pid, path, &uid))
      continue;

    if (strcmp(path, this_path) != 0)
      continue;

    if (uid == this_uid)
      ++this_user_count;
    else
      ++other_user_count;
  }

  UMA_HISTOGRAM_COUNTS_100("OSX.OtherInstances.ThisUser", this_user_count);
  UMA_HISTOGRAM_COUNTS_100("OSX.OtherInstances.OtherUser", other_user_count);
}

// Used for UMA; never alter existing values.
enum class FastUserSwitchEvent {
  kUserDidBecomeActiveEvent,
  kUserDidBecomeInactiveEvent,
  kMaxValue = kUserDidBecomeInactiveEvent,
};

void LogFastUserSwitchStat(FastUserSwitchEvent event) {
  UMA_HISTOGRAM_ENUMERATION("OSX.FastUserSwitch", event);
}

void InstallFastUserSwitchStatRecorder() {
  NSNotificationCenter* notification_center =
      [[NSWorkspace sharedWorkspace] notificationCenter];
  [notification_center
      addObserverForName:NSWorkspaceSessionDidBecomeActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* note) {
                LogFastUserSwitchStat(
                    FastUserSwitchEvent::kUserDidBecomeActiveEvent);
              }];
  [notification_center
      addObserverForName:NSWorkspaceSessionDidResignActiveNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* note) {
                LogFastUserSwitchStat(
                    FastUserSwitchEvent::kUserDidBecomeInactiveEvent);
              }];
}

// Records various bits of information about the local Chromium installation in
// UMA.
void RecordInstallationStats() {
  RecordFilesystemStats();
  RecordInstanceStats();
  InstallFastUserSwitchStatRecorder();
}

}  // namespace

// ChromeBrowserMainPartsMac ---------------------------------------------------

ChromeBrowserMainPartsMac::ChromeBrowserMainPartsMac(
    const content::MainFunctionParams& parameters,
    ChromeFeatureListCreator* chrome_feature_list_creator)
    : ChromeBrowserMainPartsPosix(parameters,
                                  chrome_feature_list_creator) {}

ChromeBrowserMainPartsMac::~ChromeBrowserMainPartsMac() {
}

int ChromeBrowserMainPartsMac::PreEarlyInitialization() {
  if (base::mac::WasLaunchedAsLoginItemRestoreState()) {
    base::CommandLine* singleton_command_line =
        base::CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kRestoreLastSession);
  } else if (base::mac::WasLaunchedAsHiddenLoginItem()) {
    base::CommandLine* singleton_command_line =
        base::CommandLine::ForCurrentProcess();
    singleton_command_line->AppendSwitch(switches::kNoStartupWindow);
  }

  return ChromeBrowserMainPartsPosix::PreEarlyInitialization();
}

void ChromeBrowserMainPartsMac::PostEarlyInitialization() {
  ui::NativeThemeMac::MaybeUpdateBrowserAppearance();
  ChromeBrowserMainPartsPosix::PostEarlyInitialization();
}

void ChromeBrowserMainPartsMac::PreMainMessageLoopStart() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::PRE_MAIN_MESSAGE_LOOP_START);
  ChromeBrowserMainPartsPosix::PreMainMessageLoopStart();

  // ChromeBrowserMainParts should have loaded the resource bundle by this
  // point (needed to load the nib).
  CHECK(ui::ResourceBundle::HasSharedInstance());

  // This is a no-op if the KeystoneRegistration framework is not present.
  // The framework is only distributed with branded Google Chrome builds.
  [[KeystoneGlue defaultKeystoneGlue] registerWithKeystone];

  // Disk image installation is sort of a first-run task, so it shares the
  // no first run switches.
  //
  // This needs to be done after the resource bundle is initialized (for
  // access to localizations in the UI) and after Keystone is initialized
  // (because the installation may need to promote Keystone) but before the
  // app controller is set up (and thus before MainMenu.nib is loaded, because
  // the app controller assumes that a browser has been set up and will crash
  // upon receipt of certain notifications if no browser exists), before
  // anyone tries doing anything silly like firing off an import job, and
  // before anything creating preferences like Local State in order for the
  // relaunched installed application to still consider itself as first-run.
  if (!first_run::IsFirstRunSuppressed(parsed_command_line())) {
    if (MaybeInstallFromDiskImage()) {
      // The application was installed and the installed copy has been
      // launched.  This process is now obsolete.  Exit.
      exit(0);
    }
  }

  // Create the app delegate. This object is intentionally leaked as a global
  // singleton. It is accessed through -[NSApp delegate].
  AppController* app_controller = [[AppController alloc] init];
  [NSApp setDelegate:app_controller];

  chrome::BuildMainMenu(NSApp, app_controller,
                        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), false);
  [app_controller mainMenuCreated];

  // Initialize the OSCrypt.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  OSCrypt::Init(local_state);
}

void ChromeBrowserMainPartsMac::PostMainMessageLoopStart() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::POST_MAIN_MESSAGE_LOOP_START);
  ChromeBrowserMainPartsPosix::PostMainMessageLoopStart();

  RecordInstallationStats();
}

void ChromeBrowserMainPartsMac::PreProfileInit() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::PRE_PROFILE_INIT);
  ChromeBrowserMainPartsPosix::PreProfileInit();

  // This is called here so that the app shim socket is only created after
  // taking the singleton lock.
  g_browser_process->platform_part()->app_shim_host_manager()->Init();
}

void ChromeBrowserMainPartsMac::PostProfileInit() {
  MacStartupProfiler::GetInstance()->Profile(
      MacStartupProfiler::POST_PROFILE_INIT);
  ChromeBrowserMainPartsPosix::PostProfileInit();

  g_browser_process->metrics_service()->RecordBreakpadRegistration(
      crash_reporter::GetUploadsEnabled());

  if (first_run::IsChromeFirstRun())
    EnsureMetadataNeverIndexFile(user_data_dir());

  // Activation of Keystone is not automatic but done in response to the
  // counting and reporting of profiles.
  KeystoneGlue* glue = [KeystoneGlue defaultKeystoneGlue];
  if (glue && ![glue isRegisteredAndActive]) {
    // If profile loading has failed, we still need to handle other tasks
    // like marking of the product as active.
    [glue updateProfileCountsWithNumProfiles:0
                         numSignedInProfiles:0];
  }
}

void ChromeBrowserMainPartsMac::DidEndMainMessageLoop() {
  AppController* appController =
      base::mac::ObjCCastStrict<AppController>([NSApp delegate]);
  [appController didEndMainMessageLoop];
}
