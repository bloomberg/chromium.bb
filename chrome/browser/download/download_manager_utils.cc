// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager_utils.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "content/public/browser/download_request_utils.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_manager_service.h"
#endif

namespace {
// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}
}  // namespace

download::InProgressDownloadManager*
DownloadManagerUtils::RetrieveInProgressDownloadManager(Profile* profile) {
  // TODO(qinmin): use the profile to retrieve SimpleFactoryKey and
  // create SimpleDownloadManagerCoordinator.
#if defined(OS_ANDROID)
  download::InProgressDownloadManager* manager =
      DownloadManagerService::GetInstance()->RetriveInProgressDownloadManager(
          profile);
  if (manager)
    return manager;
#endif
  return new download::InProgressDownloadManager(
      nullptr,
      profile->IsOffTheRecord() ? base::FilePath() : profile->GetPath(),
      base::BindRepeating(&IgnoreOriginSecurityCheck),
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe));
}
