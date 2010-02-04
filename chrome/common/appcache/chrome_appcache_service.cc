// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/appcache/chrome_appcache_service.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/notification_service.h"
#include "webkit/appcache/appcache_thread.h"

static bool has_initialized_thread_ids;

ChromeAppCacheService::ChromeAppCacheService(
    const FilePath& profile_path,
    ChromeURLRequestContext* request_context) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(request_context);

  if (!has_initialized_thread_ids) {
    has_initialized_thread_ids = true;
    appcache::AppCacheThread::InitIDs(ChromeThread::DB, ChromeThread::IO);
  }

  host_contents_settings_map_ = request_context->host_content_settings_map();
  registrar_.Add(
      this, NotificationType::PURGE_MEMORY, NotificationService::AllSources());

  // Init our base class.
  Initialize(request_context->is_off_the_record() ?
      FilePath() : profile_path.Append(chrome::kAppCacheDirname));
  set_request_context(request_context);
}

ChromeAppCacheService::~ChromeAppCacheService() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
}

// static
void ChromeAppCacheService::ClearLocalState(const FilePath& profile_path) {
  file_util::Delete(profile_path.Append(chrome::kAppCacheDirname), true);
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
