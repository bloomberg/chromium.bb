// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/version_loader.h"

#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Converts const string* to const string&.
void VersionLoaderCallbackHelper(
    base::Callback<void(const std::string&)> callback,
    const std::string* version) {
  callback.Run(*version);
}

}  // namespace

namespace chromeos {

// File to look for version number in.
static const char kPathVersion[] = "/etc/lsb-release";

// File to look for firmware number in.
static const char kPathFirmware[] = "/var/log/bios_info.txt";

VersionLoader::VersionLoader() : backend_(new Backend()) {}

VersionLoader::~VersionLoader() {}

// Beginning of line we look for that gives full version number.
// Format: x.x.xx.x (Developer|Official build extra info) board info
// static
const char VersionLoader::kFullVersionPrefix[] =
    "CHROMEOS_RELEASE_DESCRIPTION=";

// Same but for short version (x.x.xx.x).
// static
const char VersionLoader::kVersionPrefix[] = "CHROMEOS_RELEASE_VERSION=";

// Beginning of line we look for that gives the firmware version.
const char VersionLoader::kFirmwarePrefix[] = "version";

CancelableTaskTracker::TaskId VersionLoader::GetVersion(
    VersionFormat format,
    const GetVersionCallback& callback,
    CancelableTaskTracker* tracker) {
  std::string* version = new std::string();
  return tracker->PostTaskAndReply(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&Backend::GetVersion, backend_.get(), format, version),
      base::Bind(&VersionLoaderCallbackHelper, callback, base::Owned(version)));
}

CancelableTaskTracker::TaskId VersionLoader::GetFirmware(
    const GetFirmwareCallback& callback,
    CancelableTaskTracker* tracker) {
  std::string* firmware = new std::string();
  return tracker->PostTaskAndReply(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&Backend::GetFirmware, backend_.get(), firmware),
      base::Bind(&VersionLoaderCallbackHelper,
                 callback, base::Owned(firmware)));
}

// static
std::string VersionLoader::ParseVersion(const std::string& contents,
                                        const std::string& prefix) {
  // The file contains lines such as:
  // XXX=YYY
  // AAA=ZZZ
  // Split the lines and look for the one that starts with prefix. The version
  // file is small, which is why we don't try and be tricky.
  std::vector<std::string> lines;
  base::SplitString(contents, '\n', &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    if (StartsWithASCII(lines[i], prefix, false)) {
      std::string version = lines[i].substr(std::string(prefix).size());
      if (version.size() > 1 && version[0] == '"' &&
          version[version.size() - 1] == '"') {
        // Trim trailing and leading quotes.
        version = version.substr(1, version.size() - 2);
      }
      return version;
    }
  }
  return std::string();
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

  std::string contents;
  const base::FilePath file_path(kPathVersion);
  if (file_util::ReadFileToString(file_path, &contents)) {
    *version = ParseVersion(
        contents,
        (format == VERSION_FULL) ? kFullVersionPrefix : kVersionPrefix);
  }

  if (format == VERSION_SHORT_WITH_DATE) {
    base::PlatformFileInfo fileinfo;
    if (file_util::GetFileInfo(file_path, &fileinfo)) {
      base::Time::Exploded ctime;
      fileinfo.creation_time.UTCExplode(&ctime);
      *version += base::StringPrintf("-%02u.%02u.%02u",
                                     ctime.year % 100,
                                     ctime.month,
                                     ctime.day_of_month);
    }
  }
}

void VersionLoader::Backend::GetFirmware(std::string* firmware) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::string contents;
  const base::FilePath file_path(kPathFirmware);
  if (file_util::ReadFileToString(file_path, &contents)) {
    *firmware = ParseFirmware(contents);
  }
}

}  // namespace chromeos
