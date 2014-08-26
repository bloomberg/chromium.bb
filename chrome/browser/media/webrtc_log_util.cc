// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_log_util.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc_log_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

namespace {

const int kDaysToKeepLogs = 5;

// Remove any empty entries from the log list. One line is one log entry, see
// WebRtcLogUploader::AddLocallyStoredLogInfoToUploadListFile for more
// information about the format.
void RemoveEmptyEntriesInLogList(std::string* log_list) {
  static const char kEmptyLine[] = ",,\n";
  size_t pos = 0;
  do {
    pos = log_list->find(kEmptyLine, pos);
    if (pos == std::string::npos)
      break;
    DCHECK(pos == 0 || (*log_list)[pos - 1] == '\n');
    log_list->erase(pos, arraysize(kEmptyLine) - 1);
  } while (true);
}

}  // namespace

// static
void WebRtcLogUtil::DeleteOldWebRtcLogFiles(const base::FilePath& log_dir) {
  DeleteOldAndRecentWebRtcLogFiles(log_dir, base::Time::Max());
}

// static
void WebRtcLogUtil::DeleteOldAndRecentWebRtcLogFiles(
    const base::FilePath& log_dir,
    const base::Time& delete_begin_time) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  if (!base::PathExists(log_dir)) {
    // This will happen if no logs have been stored or uploaded.
    DVLOG(3) << "Could not find directory: " << log_dir.value();
    return;
  }

  const base::Time now = base::Time::Now();
  const base::TimeDelta time_to_keep_logs =
      base::TimeDelta::FromDays(kDaysToKeepLogs);

  base::FilePath log_list_path =
      WebRtcLogList::GetWebRtcLogListFileForDirectory(log_dir);
  std::string log_list;
  const bool update_log_list = base::PathExists(log_list_path);
  if (update_log_list) {
    bool read_ok = base::ReadFileToString(log_list_path, &log_list);
    DPCHECK(read_ok);
  }

  base::FileEnumerator log_files(log_dir, false, base::FileEnumerator::FILES);
  bool delete_ok = true;
  for (base::FilePath name = log_files.Next(); !name.empty();
       name = log_files.Next()) {
    if (name == log_list_path)
      continue;
    base::FileEnumerator::FileInfo file_info(log_files.GetInfo());
    base::TimeDelta file_age = now - file_info.GetLastModifiedTime();
    if (file_age > time_to_keep_logs ||
        (!delete_begin_time.is_max() &&
         file_info.GetLastModifiedTime() > delete_begin_time)) {
      if (!base::DeleteFile(name, false))
        delete_ok = false;

      // Remove the local ID from the log list file. The ID is guaranteed to be
      // unique.
      std::string id = file_info.GetName().RemoveExtension().MaybeAsASCII();
      size_t id_pos = log_list.find(id);
      if (id_pos == std::string::npos)
        continue;
      log_list.erase(id_pos, id.size());
    }
  }

  if (!delete_ok)
    LOG(WARNING) << "Could not delete all old WebRTC logs.";

  RemoveEmptyEntriesInLogList(&log_list);

  if (update_log_list) {
    int written = base::WriteFile(log_list_path, &log_list[0], log_list.size());
    DPCHECK(written == static_cast<int>(log_list.size()));
  }
}

// static
void WebRtcLogUtil::DeleteOldWebRtcLogFilesForAllProfiles() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ProfileInfoCache& profile_cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profiles_count = profile_cache.GetNumberOfProfiles();
  for (size_t i = 0; i < profiles_count; ++i) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&DeleteOldWebRtcLogFiles,
                   WebRtcLogList::GetWebRtcLogDirectoryForProfile(
                       profile_cache.GetPathOfProfileAtIndex(i))));
  }
}
