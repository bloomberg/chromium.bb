// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service.h"

#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/download_manager.h"

using content::BrowserContext;
using content::DownloadManager;
using content::DownloadManagerDelegate;

DownloadService::DownloadService(Profile* profile)
    : download_manager_created_(false),
      profile_(profile) {
}

DownloadService::~DownloadService() {}

void DownloadService::OnManagerCreated(
    const DownloadService::OnManagerCreatedCallback& cb) {
  if (download_manager_created_) {
    DownloadManager* dm = BrowserContext::GetDownloadManager(profile_);
    cb.Run(dm);
  } else {
    on_manager_created_callbacks_.push_back(cb);
  }
}

DownloadManagerDelegate* DownloadService::GetDownloadManagerDelegate() {
  DCHECK(!download_manager_created_);
  download_manager_created_ = true;

  // In case the delegate has already been set by
  // SetDownloadManagerDelegateForTesting.
  if (!manager_delegate_.get())
    manager_delegate_ = new ChromeDownloadManagerDelegate(profile_);

  DownloadManager* dm = BrowserContext::GetDownloadManager(profile_);
  manager_delegate_->SetDownloadManager(dm);

  // Include this download manager in the set monitored by the
  // global status updater.
  g_browser_process->download_status_updater()->AddManager(dm);

  download_manager_created_ = true;
  for (std::vector<OnManagerCreatedCallback>::iterator cb
        = on_manager_created_callbacks_.begin();
        cb != on_manager_created_callbacks_.end(); ++cb) {
    cb->Run(dm);
  }
  on_manager_created_callbacks_.clear();

  return manager_delegate_.get();
}

bool DownloadService::HasCreatedDownloadManager() {
  return download_manager_created_;
}

int DownloadService::DownloadCount() const {
  if (!download_manager_created_)
    return 0;
  return BrowserContext::GetDownloadManager(profile_)->InProgressCount();
}

// static
int DownloadService::DownloadCountAllProfiles() {
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());

  int count = 0;
  for (std::vector<Profile*>::iterator it = profiles.begin();
       it < profiles.end(); ++it) {
    count += DownloadServiceFactory::GetForProfile(*it)->DownloadCount();
    if ((*it)->HasOffTheRecordProfile())
      count += DownloadServiceFactory::GetForProfile(
          (*it)->GetOffTheRecordProfile())->DownloadCount();
  }

  return count;
}

void DownloadService::SetDownloadManagerDelegateForTesting(
    ChromeDownloadManagerDelegate* new_delegate) {
  // Guarantee everything is properly initialized.
  DownloadManager* dm = BrowserContext::GetDownloadManager(profile_);
  dm->SetDelegate(new_delegate);
  new_delegate->SetDownloadManager(dm);
  manager_delegate_ = new_delegate;
}

void DownloadService::Shutdown() {
  if (download_manager_created_) {
    // Normally the DownloadManager would be shutdown later, after the Profile
    // goes away and BrowserContext's destructor runs. But that would be too
    // late for us since we need to use the profile (indirectly through history
    // code) when the DownloadManager is shutting down. So we shut it down
    // manually earlier. See http://crbug.com/131692
    BrowserContext::GetDownloadManager(profile_)->Shutdown();
  }
  manager_delegate_.release();
}
