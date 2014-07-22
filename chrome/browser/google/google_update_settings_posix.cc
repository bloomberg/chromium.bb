// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/common/chrome_paths.h"

namespace {

base::LazyInstance<std::string>::Leaky g_posix_client_id =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky g_posix_client_id_lock =
    LAZY_INSTANCE_INITIALIZER;

// File name used in the user data dir to indicate consent.
const char kConsentToSendStats[] = "Consent To Send Stats";

}  // namespace

// static
bool GoogleUpdateSettings::GetCollectStatsConsent() {
  base::FilePath consent_file;
  PathService::Get(chrome::DIR_USER_DATA, &consent_file);
  consent_file = consent_file.Append(kConsentToSendStats);

  if (!base::DirectoryExists(consent_file.DirName()))
    return false;

  std::string tmp_guid;
  bool consented = base::ReadFileToString(consent_file, &tmp_guid);
  if (consented) {
    base::AutoLock lock(g_posix_client_id_lock.Get());
    g_posix_client_id.Get().assign(tmp_guid);
  }
  return consented;
}

// static
bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  base::FilePath consent_dir;
  PathService::Get(chrome::DIR_USER_DATA, &consent_dir);
  if (!base::DirectoryExists(consent_dir))
    return false;

  base::AutoLock lock(g_posix_client_id_lock.Get());

  base::FilePath consent_file = consent_dir.AppendASCII(kConsentToSendStats);
  if (consented) {
    if ((!base::PathExists(consent_file)) ||
        (base::PathExists(consent_file) && !g_posix_client_id.Get().empty())) {
      const char* c_str = g_posix_client_id.Get().c_str();
      int size = g_posix_client_id.Get().size();
      return base::WriteFile(consent_file, c_str, size) == size;
    }
  } else {
    g_posix_client_id.Get().clear();
    return base::DeleteFile(consent_file, false);
  }
  return true;
}

// static
// TODO(gab): Implement storing/loading for all ClientInfo fields on POSIX.
scoped_ptr<metrics::ClientInfo> GoogleUpdateSettings::LoadMetricsClientInfo() {
  scoped_ptr<metrics::ClientInfo> client_info(new metrics::ClientInfo);

  base::AutoLock lock(g_posix_client_id_lock.Get());
  if (g_posix_client_id.Get().empty())
    return scoped_ptr<metrics::ClientInfo>();
  client_info->client_id = g_posix_client_id.Get();

  return client_info.Pass();
}

// static
// TODO(gab): Implement storing/loading for all ClientInfo fields on POSIX.
void GoogleUpdateSettings::StoreMetricsClientInfo(
    const metrics::ClientInfo& client_info) {
  // Make sure that user has consented to send crashes.
  if (!GoogleUpdateSettings::GetCollectStatsConsent())
    return;

  {
    // Since user has consented, write the metrics id to the file.
    base::AutoLock lock(g_posix_client_id_lock.Get());
    g_posix_client_id.Get() = client_info.client_id;
  }
  GoogleUpdateSettings::SetCollectStatsConsent(true);
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
