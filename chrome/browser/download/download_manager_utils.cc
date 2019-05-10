// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager_utils.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/common/service_manager_connection.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_manager_service.h"
#endif

namespace {
// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

// Some ChromeOS browser tests doesn't initialize DownloadManager when profile
// is created, and cause the download request to fail. This method helps us
// ensure that the DownloadManager will be created after profile creation.
void GetDownloadManagerOnProfileCreation(Profile* profile) {
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile);
  DCHECK(manager);
}

service_manager::Connector* GetServiceConnector() {
  auto* connection = content::ServiceManagerConnection::GetForProcess();
  if (!connection)
    return nullptr;
  return connection->GetConnector();
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
      base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
      GetServiceConnector());
}

void DownloadManagerUtils::InitializeSimpleDownloadManager(
    SimpleFactoryKey* key) {
#if defined(OS_ANDROID)
  if (!g_browser_process) {
    DownloadManagerService::GetInstance()->CreateInProgressDownloadManager();
    return;
  }
#endif
  FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
      key, base::BindOnce(&GetDownloadManagerOnProfileCreation));
}
