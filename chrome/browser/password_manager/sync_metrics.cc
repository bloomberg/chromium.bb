// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/sync_metrics.h"

#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"

namespace password_manager_sync_metrics {

std::string GetSyncUsername(Profile* profile) {
  // If sync is set up, return early if we aren't syncing passwords.
  if (ProfileSyncServiceFactory::HasProfileSyncService(profile)) {
    ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile);
    if (!sync_service->GetPreferredDataTypes().Has(syncer::PASSWORDS))
      return std::string();
  }

  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);

  if (!signin_manager)
    return std::string();

  return signin_manager->GetAuthenticatedUsername();
}

bool IsSyncAccountCredential(Profile* profile,
                             const std::string& username,
                             const std::string& origin) {
  if (origin != GaiaUrls::GetInstance()->gaia_url().GetOrigin().spec())
    return false;

  return gaia::AreEmailsSame(username, GetSyncUsername(profile));
}

}  // namespace password_manager_sync_metrics
