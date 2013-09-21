// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/cf_migration.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/win/registry.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_main.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

bool LaunchChromeFrameMigrationProcess(
    const ProductState& chrome_frame_product,
    const CommandLine& command_line,
    const base::FilePath& installer_directory,
    bool system_level) {
  // Before running the migration, mutate the CF ap value to include a
  // "-migrate" beacon. This beacon value will be cleaned up by the "ap"
  // cleanup in MigrateGoogleUpdateStateMultiToSingle that calls
  // ChannelInfo::RemoveAllModifiersAndSuffixes().
  if (chrome_frame_product.is_multi_install()) {
    const HKEY root = system_level ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    base::win::RegKey state_key;
    installer::ChannelInfo channel_info;
    BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_FRAME);

    LONG result = state_key.Open(root, dist->GetStateKey().c_str(),
                                 KEY_QUERY_VALUE | KEY_SET_VALUE);
    if (result != ERROR_SUCCESS || !channel_info.Initialize(state_key)) {
      LOG(ERROR) << "Failed to read CF channel to store beacon.";
    } else if (!channel_info.SetMigratingSuffix(true)) {
      LOG(WARNING) << "CF already has migration beacon in channel.";
    } else {
      VLOG(1) << "Writing CF migration beacon to channel: "
              << channel_info.value();
      channel_info.Write(&state_key);
    }
  }

  // Call the installed setup.exe using the current installer command line and
  // adding the migration flags. This seems like it could be unsafe, here's why
  // it's safe today:
  // 1) MigrateChromeFrameInChildProcess is called only during a multi update.
  // 2) Multi update processing occurs after HandleNonInstallCmdLineOptions is
  //    called.
  // 3) Setup exits if there were any non-install command line options handled.
  // 4) Thus, the command line being copied will have no non-install command
  //    line options at time of copying.
  // 5) kMigrateChromeFrame is a non-install command line option.
  // 6) Thus, it will be handled (and the child setup process will exit) before
  //    the child setup process acts on any other flags on the command line.
  // 7) Furthermore, --uncompressed-archive takes precedence over
  //    --install-archive, so it is safe to add the former to the command line
  //    without removing the latter.
  CommandLine setup_cmd(command_line);
  setup_cmd.SetProgram(installer_directory.Append(installer::kSetupExe));
  setup_cmd.AppendSwitchPath(
      switches::kUncompressedArchive,
      installer_directory.Append(installer::kChromeArchive));
  setup_cmd.AppendSwitch(switches::kMigrateChromeFrame);

  VLOG(1) << "Running Chrome Frame migration process with command line: "
          << setup_cmd.GetCommandLineString();

  base::LaunchOptions options;
  options.force_breakaway_from_job_ = true;
  if (!base::LaunchProcess(setup_cmd, options, NULL)) {
    PLOG(ERROR) << "Launching Chrome Frame migration process failed. "
                << "(Command line: " << setup_cmd.GetCommandLineString() << ")";
    return false;
  }

  return true;
}

InstallStatus MigrateChromeFrame(const InstallationState& original_state,
                                 InstallerState* installer_state) {
  const bool system_level = installer_state->system_install();

  // Nothing to do if multi-install Chrome Frame is not installed.
  const ProductState* multi_chrome_frame = original_state.GetProductState(
      system_level, BrowserDistribution::CHROME_FRAME);
  if (!multi_chrome_frame || !multi_chrome_frame->is_multi_install())
    return INVALID_STATE_FOR_OPTION;

  // Install SxS Chrome Frame.
  InstallerState install_gcf(installer_state->level());
  {
    scoped_ptr<Product> chrome_frame(
        new Product(BrowserDistribution::GetSpecificDistribution(
            BrowserDistribution::CHROME_FRAME)));
    install_gcf.AddProduct(&chrome_frame);
  }
  DCHECK(!install_gcf.is_multi_install());

  ArchiveType archive_type = UNKNOWN_ARCHIVE_TYPE;
  bool delegated_to_existing = false;
  InstallStatus install_status = InstallProductsHelper(
      original_state, *CommandLine::ForCurrentProcess(),
      MasterPreferences::ForCurrentProcess(), install_gcf,
      NULL, &archive_type, &delegated_to_existing);

  if (!InstallUtil::GetInstallReturnCode(install_status)) {
    // Migration was successful. There's no turning back now. The multi-install
    // npchrome_frame.dll and/or chrome.exe may still be in use at this point,
    // although the user-level helper will not be. It is not safe to delete the
    // multi-install binaries until npchrome_frame.dll and chrome.exe are no
    // longer in use. The remaining tasks here are best-effort. Failure does not
    // do any harm.
    MigrateGoogleUpdateStateMultiToSingle(system_level,
                                          BrowserDistribution::CHROME_FRAME,
                                          original_state);
  }

  return install_status;
}

}  // namespace installer
