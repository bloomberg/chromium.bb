// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service.h"

#include "base/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_history.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_ui_controller.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/download_manager.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/downloads/downloads_api.h"
#endif

using content::BrowserContext;
using content::DownloadManager;
using content::DownloadManagerDelegate;

DownloadService::DownloadService(Profile* profile)
    : download_manager_created_(false),
      profile_(profile) {
}

DownloadService::~DownloadService() {}

ChromeDownloadManagerDelegate* DownloadService::GetDownloadManagerDelegate() {
  DownloadManager* manager = BrowserContext::GetDownloadManager(profile_);
  // If we've already created the delegate, just return it.
  if (download_manager_created_) {
    DCHECK(static_cast<DownloadManagerDelegate*>(manager_delegate_.get()) ==
           manager->GetDelegate());
    return manager_delegate_.get();
  }
  download_manager_created_ = true;

  // In case the delegate has already been set by
  // SetDownloadManagerDelegateForTesting.
  if (!manager_delegate_.get())
    manager_delegate_.reset(new ChromeDownloadManagerDelegate(profile_));

  manager_delegate_->SetDownloadManager(manager);

#if defined(ENABLE_EXTENSIONS)
  extension_event_router_.reset(
      new extensions::ExtensionDownloadsEventRouter(profile_, manager));
#endif

  if (!profile_->IsOffTheRecord()) {
    HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile_, Profile::EXPLICIT_ACCESS);
    history->GetNextDownloadId(
        manager_delegate_->GetDownloadIdReceiverCallback());
    download_history_.reset(new DownloadHistory(
        manager,
        scoped_ptr<DownloadHistory::HistoryAdapter>(
            new DownloadHistory::HistoryAdapter(history))));
  }

  // Pass an empty delegate when constructing the DownloadUIController. The
  // default delegate does all the notifications we need.
  download_ui_.reset(new DownloadUIController(
      manager, scoped_ptr<DownloadUIController::Delegate>()));

  // Include this download manager in the set monitored by the
  // global status updater.
  g_browser_process->download_status_updater()->AddManager(manager);

  return manager_delegate_.get();
}

DownloadHistory* DownloadService::GetDownloadHistory() {
  if (!download_manager_created_) {
    GetDownloadManagerDelegate();
  }
  DCHECK(download_manager_created_);
  return download_history_.get();
}

bool DownloadService::HasCreatedDownloadManager() {
  return download_manager_created_;
}

int DownloadService::NonMaliciousDownloadCount() const {
  if (!download_manager_created_)
    return 0;
  return BrowserContext::GetDownloadManager(profile_)->
      NonMaliciousInProgressCount();
}

void DownloadService::CancelDownloads() {
  if (!download_manager_created_)
    return;

  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(profile_);
  DownloadManager::DownloadVector downloads;
  download_manager->GetAllDownloads(&downloads);
  for (DownloadManager::DownloadVector::iterator it = downloads.begin();
       it != downloads.end();
       ++it) {
    if ((*it)->GetState() == content::DownloadItem::IN_PROGRESS)
      (*it)->Cancel(false);
  }
}

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

void DownloadService::SetDownloadManagerDelegateForTesting(
    scoped_ptr<ChromeDownloadManagerDelegate> new_delegate) {
  manager_delegate_.swap(new_delegate);
  DownloadManager* dm = BrowserContext::GetDownloadManager(profile_);
  dm->SetDelegate(manager_delegate_.get());
  manager_delegate_->SetDownloadManager(dm);
  if (new_delegate)
    new_delegate->Shutdown();
}

bool DownloadService::IsShelfEnabled() {
#if defined(OS_ANDROID)
  return true;
#else
  return !extension_event_router_ ||
         extension_event_router_->IsShelfEnabled();
#endif
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
#if defined(ENABLE_EXTENSIONS)
  extension_event_router_.reset();
#endif
  manager_delegate_.reset();
  download_history_.reset();
}
