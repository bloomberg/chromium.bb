// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_listener.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/global_request_id.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "net/url_request/url_request.h"

BlacklistListener::BlacklistListener(ResourceQueue* resource_queue)
    : resource_queue_(resource_queue) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  registrar_.Add(this,
                 NotificationType::BLACKLIST_MANAGER_ERROR,
                 NotificationService::AllSources());
  registrar_.Add(this,
                 NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED,
                 NotificationService::AllSources());
}

BlacklistListener::~BlacklistListener() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void BlacklistListener::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  switch (type.value) {
    // TODO(phajdan.jr): Do not resume requests silently after an error.
    case NotificationType::BLACKLIST_MANAGER_ERROR:
    case NotificationType::BLACKLIST_MANAGER_BLACKLIST_READ_FINISHED:
      {
        Profile* profile = Source<Profile>(source).ptr();
        ChromeThread::PostTask(ChromeThread::IO, FROM_HERE,
            NewRunnableMethod(this, &BlacklistListener::StartDelayedRequests,
                              profile->GetBlacklistManager()));
      }
      break;
    default:
      NOTREACHED();
  }
}

bool BlacklistListener::ShouldDelayRequest(
    URLRequest* request,
    const ResourceDispatcherHostRequestInfo& request_info,
    const GlobalRequestID& request_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  BlacklistRequestInfo* blacklist_request_info =
      BlacklistRequestInfo::FromURLRequest(request);
  const BlacklistManager* blacklist_manager =
      blacklist_request_info->GetBlacklistManager();

  if (blacklist_manager->GetCompiledBlacklist()) {
    // We have a blacklist ready. No need to delay the request.
    return false;
  }

  delayed_requests_[blacklist_manager].push_back(request_id);
  return true;
}

void BlacklistListener::WillShutdownResourceQueue() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  resource_queue_ = NULL;
}

void BlacklistListener::StartDelayedRequests(
    BlacklistManager* blacklist_manager) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (resource_queue_) {
    const RequestList& requests = delayed_requests_[blacklist_manager];
    for (RequestList::const_iterator i = requests.begin();
         i != requests.end(); ++i) {
      resource_queue_->StartDelayedRequest(this, *i);
    }
  }

  delayed_requests_[blacklist_manager].clear();
}
