// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_system_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/chromeos/gdata/drive_webapps_registry.h"
#include "chrome/browser/chromeos/gdata/gdata_documents_service.h"
#include "chrome/browser/chromeos/gdata/gdata_download_observer.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_sync_client.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserContext;
using content::BrowserThread;

namespace gdata {

//===================== GDataSystemService ====================================
GDataSystemService::GDataSystemService(Profile* profile)
    : profile_(profile),
      sequence_token_(BrowserThread::GetBlockingPool()->GetSequenceToken()),
      cache_(GDataCache::CreateGDataCacheOnUIThread(
          GDataCache::GetCacheRootPath(profile_),
          BrowserThread::GetBlockingPool(),
          sequence_token_)),
      documents_service_(new DocumentsService),
      file_system_(new GDataFileSystem(profile,
                                       cache(),
                                       docs_service(),
                                       sequence_token_)),
      uploader_(new GDataUploader(file_system(), docs_service())),
      download_observer_(new GDataDownloadObserver),
      sync_client_(new GDataSyncClient(profile, file_system(), cache())),
      webapps_registry_(new DriveWebAppsRegistry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataSystemService::~GDataSystemService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cache_->DestroyOnUIThread();
}

void GDataSystemService::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  sync_client_->Initialize();
  file_system_->Initialize();
  cache_->RequestInitializeOnUIThread();

  content::DownloadManager* download_manager =
    g_browser_process->download_status_updater() ?
        BrowserContext::GetDownloadManager(profile_) : NULL;
  download_observer_->Initialize(
      uploader_.get(),
      download_manager,
      cache_->GetCacheDirectoryPath(
          GDataCache::CACHE_TYPE_TMP_DOWNLOADS));
}

void GDataSystemService::Shutdown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Shut down the member objects in the reverse order of creation.
  webapps_registry_.reset();
  sync_client_.reset();
  download_observer_.reset();
  uploader_.reset();
  file_system_.reset();
  documents_service_.reset();
}

//===================== GDataSystemServiceFactory =============================

// static
GDataSystemService* GDataSystemServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GDataSystemService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GDataSystemService* GDataSystemServiceFactory::FindForProfile(
    Profile* profile) {
  return static_cast<GDataSystemService*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
GDataSystemServiceFactory* GDataSystemServiceFactory::GetInstance() {
  return Singleton<GDataSystemServiceFactory>::get();
}

GDataSystemServiceFactory::GDataSystemServiceFactory()
    : ProfileKeyedServiceFactory("GDataSystemService",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(DownloadServiceFactory::GetInstance());
}

GDataSystemServiceFactory::~GDataSystemServiceFactory() {
}

ProfileKeyedService* GDataSystemServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  GDataSystemService* service = new GDataSystemService(profile);
  service->Initialize();
  return service;
}

}  // namespace gdata
