// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"

namespace google_update {

static std::string& posix_guid() {
  CR_DEFINE_STATIC_LOCAL(std::string, guid, ());
  return guid;
}

}  // namespace google_update

// File name used in the user data dir to indicate consent.
static const char kConsentToSendStats[] = "Consent To Send Stats";

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  base::FilePath consent_file;
  PathService::Get(chrome::DIR_USER_DATA, &consent_file);
  consent_file = consent_file.Append(kConsentToSendStats);
  std::string tmp_guid;
  bool consented = file_util::ReadFileToString(consent_file, &tmp_guid);
  if (consented)
    google_update::posix_guid().assign(tmp_guid);
  return consented;
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  base::FilePath consent_dir;
  PathService::Get(chrome::DIR_USER_DATA, &consent_dir);
  if (!file_util::DirectoryExists(consent_dir))
    return false;

  base::FilePath consent_file = consent_dir.AppendASCII(kConsentToSendStats);
  if (consented) {
    if ((!file_util::PathExists(consent_file)) ||
        (file_util::PathExists(consent_file) &&
         !google_update::posix_guid().empty())) {
      const char* c_str = google_update::posix_guid().c_str();
      int size = google_update::posix_guid().size();
      return file_util::WriteFile(consent_file, c_str, size) == size;
    }
  } else {
    google_update::posix_guid().clear();
    return file_util::Delete(consent_file, false);
  }
  return true;
}

bool GoogleUpdateSettings::SetMetricsId(const std::wstring& client_id) {
  // Make sure that user has consented to send crashes.
  base::FilePath consent_dir;
  PathService::Get(chrome::DIR_USER_DATA, &consent_dir);
  if (!file_util::DirectoryExists(consent_dir) ||
      !GoogleUpdateSettings::GetCollectStatsConsent())
    return false;

  // Since user has consented, write the metrics id to the file.
  google_update::posix_guid() = base::WideToASCII(client_id);
  return GoogleUpdateSettings::SetCollectStatsConsent(true);
}

// GetLastRunTime and SetLastRunTime are not implemented for posix. Their
// current return values signal failure which the caller is designed to
// handle.

// static
int GoogleUpdateSettings::GetLastRunTime() {
  return -1;
}

// static
bool GoogleUpdateSettings::SetLastRunTime() {
  return false;
}
