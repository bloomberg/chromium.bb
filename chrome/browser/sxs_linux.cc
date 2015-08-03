// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sxs_linux.h"

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

namespace {

const char kChannelsFileName[] = "Channels";

std::string GetChannelMarkForThisExecutable() {
  std::string product_channel_name;
  version_info::Channel product_channel(chrome::GetChannel());
  switch (product_channel) {
    case version_info::Channel::UNKNOWN: {
      // Add the channel mark even for Chromium builds (which do not have
      // channel) to better handle possibility of users using Chromium builds
      // with their Google Chrome profiles. Include version string modifier
      // as additional piece of information for debugging (it can't make
      // a meaningful difference for the code since unknown does not match any
      // real channel name).
      std::string version_string_modifier(chrome::GetChannelString());
      product_channel_name = "unknown (" + version_string_modifier + ")";
      break;
    }
    case version_info::Channel::CANARY:
      product_channel_name = "canary";
      break;
    case version_info::Channel::DEV:
      product_channel_name = "dev";
      break;
    case version_info::Channel::BETA:
      product_channel_name = "beta";
      break;
    case version_info::Channel::STABLE:
      product_channel_name = "stable";
      break;
    // Rely on -Wswitch compiler warning to detect unhandled enum values.
  }

  return product_channel_name;
}

bool DoAddChannelMarkToUserDataDir(const base::FilePath& user_data_dir) {
  std::string product_channel_name(GetChannelMarkForThisExecutable());
  base::FilePath channels_path(user_data_dir.AppendASCII(kChannelsFileName));
  std::vector<std::string> user_data_dir_channels;

  // Note: failure to read the channels file is not fatal. It's possible
  // and legitimate that it doesn't exist, e.g. for new profile or for profile
  // existing before channel marks have been introduced.
  std::string channels_contents;
  if (base::ReadFileToString(channels_path, &channels_contents)) {
    base::SplitString(channels_contents, "\n",
                      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }

  if (std::find(user_data_dir_channels.begin(),
                user_data_dir_channels.end(),
                product_channel_name) != user_data_dir_channels.end()) {
    // No need to do further disk writes if our channel mark is already present.
    return true;
  }

  user_data_dir_channels.push_back(product_channel_name);
  return base::ImportantFileWriter::WriteFileAtomically(
      channels_path,
      base::JoinString(user_data_dir_channels, "\n"));
}

}  // namespace

namespace sxs_linux {

void AddChannelMarkToUserDataDir() {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    LOG(ERROR) << "Failed to get user data dir path. The profile will not be "
               << "automatically migrated for updated Linux packages.";
    return;
  }

  if (!DoAddChannelMarkToUserDataDir(user_data_dir)) {
    LOG(ERROR) << "Failed to add channel mark to the user data dir ("
               << user_data_dir.value() << "). This profile will not be "
               << "automatically migrated for updated Linux packages.";
  }
}

bool ShouldMigrateUserDataDir() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kMigrateDataDirForSxS);
}

int MigrateUserDataDir() {
  DCHECK(ShouldMigrateUserDataDir());

  base::FilePath source_path;
  if (!PathService::Get(chrome::DIR_USER_DATA, &source_path)) {
    LOG(ERROR) << "Failed to get value of chrome::DIR_USER_DATA";
    return chrome::RESULT_CODE_SXS_MIGRATION_FAILED;
  }

  base::FilePath channels_path(source_path.AppendASCII(kChannelsFileName));

  std::string channels_contents;
  if (!base::ReadFileToString(channels_path, &channels_contents)) {
    LOG(WARNING) << "Failed to read channels file.";
    return chrome::RESULT_CODE_SXS_MIGRATION_FAILED;
  }

  std::vector<std::string> user_data_dir_channels = base::SplitString(
      channels_contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (user_data_dir_channels.size() != 1) {
    LOG(WARNING) << "User data dir migration is only possible when the profile "
                 << "is only used with a single channel.";
    return chrome::RESULT_CODE_SXS_MIGRATION_FAILED;
  }

  if (user_data_dir_channels[0] != GetChannelMarkForThisExecutable()) {
    LOG(WARNING) << "User data dir migration is only possible when the profile "
                 << "is used with the same channel.";
    return chrome::RESULT_CODE_SXS_MIGRATION_FAILED;
  }

  base::FilePath target_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kMigrateDataDirForSxS);

  if (!base::Move(source_path, target_path)) {
    LOG(ERROR) << "Failed to rename '" << source_path.value()
               << "' to '" << target_path.value() << "'";
    return chrome::RESULT_CODE_SXS_MIGRATION_FAILED;
  }

  return content::RESULT_CODE_NORMAL_EXIT;
}

}  // namespace sxs_linux
