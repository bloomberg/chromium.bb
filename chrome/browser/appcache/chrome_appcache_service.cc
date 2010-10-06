// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/appcache/chrome_appcache_service.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "net/base/net_errors.h"
#include "webkit/appcache/appcache_thread.h"

static bool has_initialized_thread_ids;

// ----------------------------------------------------------------------------

ChromeAppCacheService::ChromeAppCacheService() {
}

void ChromeAppCacheService::InitializeOnIOThread(
    const FilePath& profile_path, bool is_incognito,
    scoped_refptr<HostContentSettingsMap> content_settings_map) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!has_initialized_thread_ids) {
    has_initialized_thread_ids = true;
    appcache::AppCacheThread::Init(BrowserThread::DB, BrowserThread::IO);
  }

  host_contents_settings_map_ = content_settings_map;
  registrar_.Add(
      this, NotificationType::PURGE_MEMORY, NotificationService::AllSources());

  // Init our base class.
  Initialize(
      is_incognito ? FilePath() : profile_path.Append(chrome::kAppCacheDirname),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE));
  set_appcache_policy(this);
}

ChromeAppCacheService::~ChromeAppCacheService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void ChromeAppCacheService::SetOriginQuotaInMemory(
    const GURL& origin, int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (storage())
    storage()->SetOriginQuotaInMemory(origin, quota);
}

void ChromeAppCacheService::ResetOriginQuotaInMemory(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (storage())
    storage()->ResetOriginQuotaInMemory(origin);
}

// static
void ChromeAppCacheService::ClearLocalState(const FilePath& profile_path) {
  file_util::Delete(profile_path.Append(chrome::kAppCacheDirname), true);
}

bool ChromeAppCacheService::CanLoadAppCache(const GURL& manifest_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ContentSetting setting = host_contents_settings_map_->GetContentSetting(
      manifest_url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
  // We don't prompt for read access.
  return setting != CONTENT_SETTING_BLOCK;
}

int ChromeAppCacheService::CanCreateAppCache(
    const GURL& manifest_url, net::CompletionCallback* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ContentSetting setting = host_contents_settings_map_->GetContentSetting(
      manifest_url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  DCHECK(setting != CONTENT_SETTING_DEFAULT);
  return (setting != CONTENT_SETTING_BLOCK) ? net::OK :
                                              net::ERR_ACCESS_DENIED;
}

void ChromeAppCacheService::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(type == NotificationType::PURGE_MEMORY);
  PurgeMemory();
}

// ----------------------------------------------------------------------------

static BrowserThread::ID ToBrowserThreadID(int id) {
  DCHECK(has_initialized_thread_ids);
  DCHECK(id == BrowserThread::DB || id == BrowserThread::IO);
  return static_cast<BrowserThread::ID>(id);
}

namespace appcache {

// An impl of AppCacheThread we need to provide to the appcache lib.

bool AppCacheThread::PostTask(
    int id,
    const tracked_objects::Location& from_here,
    Task* task) {
  return BrowserThread::PostTask(ToBrowserThreadID(id), from_here, task);
}

bool AppCacheThread::CurrentlyOn(int id) {
  return BrowserThread::CurrentlyOn(ToBrowserThreadID(id));
}

}  // namespace appcache
