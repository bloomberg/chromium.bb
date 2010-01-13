// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/appcache/chrome_appcache_service.h"

#include "base/file_path.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "webkit/appcache/appcache_thread.h"

static bool has_initialized_thread_ids;

ChromeAppCacheService::ChromeAppCacheService(const FilePath& data_directory,
                                             bool is_incognito) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  if (!has_initialized_thread_ids) {
    has_initialized_thread_ids = true;
    appcache::AppCacheThread::InitIDs(ChromeThread::DB, ChromeThread::IO);
  }
  Initialize(is_incognito ? FilePath()
                          : data_directory.Append(chrome::kAppCacheDirname));

  registrar_.Add(
      this, NotificationType::PURGE_MEMORY, NotificationService::AllSources());
}

ChromeAppCacheService::~ChromeAppCacheService() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
}

void ChromeAppCacheService::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  DCHECK(type == NotificationType::PURGE_MEMORY);
  PurgeMemory();
}

static ChromeThread::ID ToChromeThreadID(int id) {
  DCHECK(has_initialized_thread_ids);
  DCHECK(id == ChromeThread::DB || id == ChromeThread::IO);
  return static_cast<ChromeThread::ID>(id);
}

namespace appcache {

// An impl of AppCacheThread we need to provide to the appcache lib.

bool AppCacheThread::PostTask(
    int id,
    const tracked_objects::Location& from_here,
    Task* task) {
  return ChromeThread::PostTask(ToChromeThreadID(id), from_here, task);
}

bool AppCacheThread::CurrentlyOn(int id) {
  return ChromeThread::CurrentlyOn(ToChromeThreadID(id));
}

}  // namespace appcache
