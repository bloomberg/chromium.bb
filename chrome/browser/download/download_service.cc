// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

DownloadService::DownloadService() {
}

DownloadService::~DownloadService() {}

// static
int DownloadService::NonMaliciousDownloadCountAllProfiles() {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());

  int count = 0;
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it < profiles.end(); ++it) {
    count += DownloadServiceFactory::GetForBrowserContext(*it)->
        NonMaliciousDownloadCount();
    if ((*it)->HasOffTheRecordProfile())
      count += DownloadServiceFactory::GetForBrowserContext(
          (*it)->GetOffTheRecordProfile())->NonMaliciousDownloadCount();
  }

  return count;
}

// static
void DownloadService::CancelAllDownloads() {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it < profiles.end();
       ++it) {
    DownloadService* service =
        DownloadServiceFactory::GetForBrowserContext(*it);
    service->CancelDownloads();
  }
}

