// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/version_loader.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Beginning of line we look for that gives full version number.
// Format: x.x.xx.x (Developer|Official build extra info) board info
const char kFullVersionKey[] = "CHROMEOS_RELEASE_DESCRIPTION";

// Same but for short version (x.x.xx.x).
const char kVersionKey[] = "CHROMEOS_RELEASE_VERSION";

// Beginning of line we look for that gives the firmware version.
const char kFirmwarePrefix[] = "version";

// File to look for firmware number in.
const char kPathFirmware[] = "/var/log/bios_info.txt";

// Converts const string* to const string&.
void VersionLoaderCallbackHelper(
    base::Callback<void(const std::string&)> callback,
    const std::string* version) {
  callback.Run(*version);
}

}  // namespace

namespace chromeos {

VersionLoader::VersionLoader() : backend_(new Backend()) {}

VersionLoader::~VersionLoader() {}

base::CancelableTaskTracker::TaskId VersionLoader::GetVersion(
    VersionFormat format,
    const GetVersionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::string* version = new std::string();
  return tracker->PostTaskAndReply(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&Backend::GetVersion, backend_.get(), format, version),
      base::Bind(&VersionLoaderCallbackHelper, callback, base::Owned(version)));
}

base::CancelableTaskTracker::TaskId VersionLoader::GetFirmware(
    const GetFirmwareCallback& callback,
    base::CancelableTaskTracker* tracker) {
  std::string* firmware = new std::string();
  return tracker->PostTaskAndReply(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&Backend::GetFirmware, backend_.get(), firmware),
      base::Bind(&VersionLoaderCallbackHelper,
                 callback, base::Owned(firmware)));
}

// static
std::string VersionLoader::ParseFirmware(const std::string& contents) {
  // The file contains lines such as:
  // vendor           | ...
  // version          | ...
  // release_date     | ...
  // We don't make any assumption that the spaces between "version" and "|" is
  //   fixed. So we just match kFirmwarePrefix at the start of the line and find
  //   the first character that is not "|" or space

  std::vector<std::string> lines;
  base::SplitString(contents, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (StartsWithASCII(lines[i], kFirmwarePrefix, false)) {
      std::string str = lines[i].substr(std::string(kFirmwarePrefix).size());
      size_t found = str.find_first_not_of("| ");
      if (found != std::string::npos)
        return str.substr(found);
    }
  }
  return std::string();
}

void VersionLoader::Backend::GetVersion(VersionFormat format,
                                        std::string* version) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::string key = (format == VERSION_FULL) ? kFullVersionKey : kVersionKey;
  if (!base::SysInfo::GetLsbReleaseValue(key, version)) {
    LOG_IF(ERROR, base::SysInfo::IsRunningOnChromeOS())
        << "No LSB version key: " << key;
    *version = "0.0.0.0";
  }
  if (format == VERSION_SHORT_WITH_DATE) {
    base::Time::Exploded ctime;
    base::SysInfo::GetLsbReleaseTime().UTCExplode(&ctime);
    *version += base::StringPrintf("-%02u.%02u.%02u",
                                   ctime.year % 100,
                                   ctime.month,
                                   ctime.day_of_month);
  }
}

void VersionLoader::Backend::GetFirmware(std::string* firmware) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::string contents;
  const base::FilePath file_path(kPathFirmware);
  if (base::ReadFileToString(file_path, &contents)) {
    *firmware = ParseFirmware(contents);
  }
}

}  // namespace chromeos
