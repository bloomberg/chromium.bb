// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/lsb_release_log_source.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kInvalidLogEntry[] = "<invalid characters in log entry>";
const char kEmptyLogEntry[] = "<no value>";

typedef std::pair<std::string, std::string> KeyValuePair;

}  // namespace

namespace chromeos {

void LsbReleaseLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&LsbReleaseLogSource::ReadLSBRelease, response),
      base::Bind(callback,
                 base::Owned(response)));
}

void LsbReleaseLogSource::ReadLSBRelease(SystemLogsResponse* response) {
  DCHECK(response);

  const base::FilePath lsb_release_file("/etc/lsb-release");
  std::string lsb_data;
  bool read_success = file_util::ReadFileToString(lsb_release_file, &lsb_data);
  // if we were using an internal temp file, the user does not need the
  // logs to stay past the ReadFile call - delete the file
  if (!read_success) {
    LOG(ERROR) << "Can't access /etc/lsb-release file.";
    return;
  }
  ParseLSBRelease(lsb_data, response);
}

void LsbReleaseLogSource::ParseLSBRelease(const std::string& lsb_data,
                                          SystemLogsResponse* response) {
  std::vector<KeyValuePair> pairs;
  base::SplitStringIntoKeyValuePairs(lsb_data, '=', '\n', &pairs);
  for (size_t i = 0; i  < pairs.size(); ++i) {
    std::string key, value;
    TrimWhitespaceASCII(pairs[i].first, TRIM_ALL, &key);
    TrimWhitespaceASCII(pairs[i].second, TRIM_ALL, &value);

    if (key.empty())
      continue;
    if (!IsStringUTF8(value) || !IsStringUTF8(key)) {
      LOG(WARNING) << "Invalid characters in system log entry: " << key;
      (*response)[key] = kInvalidLogEntry;
      continue;
    }

    if (value.empty())
      (*response)[key] = kEmptyLogEntry;
    else
      (*response)[key] = value;
  }
}

}  // namespace chromeos

