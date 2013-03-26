// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/local_omnibox_popup_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      most_visited_item_cache_(kMaxInstantMostVisitedItemCacheSize) {
  // Stub for unit tests.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  instant_io_context_ = new InstantIOContext();

  if (profile_ && profile_->GetResourceContext()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::SetUserDataOnIO,
                   profile->GetResourceContext(), instant_io_context_));
  }

  // Set up the data sources that Instant uses on the NTP.
#if defined(ENABLE_THEMES)
  content::URLDataSource::Add(profile, new ThemeSource(profile));
#endif
  content::URLDataSource::Add(profile, new ThumbnailSource(profile));
  content::URLDataSource::Add(profile, new FaviconSource(
      profile, FaviconSource::FAVICON));
  content::URLDataSource::Add(profile, new LocalOmniboxPopupSource());
  content::URLDataSource::Add(profile, new LocalNtpSource());
}

InstantService::~InstantService() {
}

// static
const std::string InstantService::MaybeTranslateInstantPathOnUI(
    Profile* profile, const std::string& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service)
    return path;

  InstantRestrictedID restricted_id = 0;
  DCHECK_EQ(sizeof(InstantRestrictedID), sizeof(int));
  if (base::StringToInt(path, &restricted_id)) {
    InstantMostVisitedItem item;
    if (instant_service->GetMostVisitedItemForID(restricted_id, &item))
      return item.url.spec();
  }
  return path;
}

const std::string InstantService::MaybeTranslateInstantPathOnIO(
    const net::URLRequest* request, const std::string& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  InstantRestrictedID restricted_id = 0;
  DCHECK_EQ(sizeof(InstantRestrictedID), sizeof(int));
  if (base::StringToInt(path, &restricted_id)) {
    GURL url;
    if (InstantIOContext::GetURLForMostVisitedItemID(request,
                                                     restricted_id,
                                                     &url)) {
      return url.spec();
    }
  }
  return path;
}

// static
bool InstantService::IsInstantPath(const GURL& url) {
  // Strip leading slash.
  std::string path = url.path().substr(1);

  // Check that path is of Most Visited item ID form.
  InstantRestrictedID dummy = 0;
  return base::StringToInt(path, &dummy);
}

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::AddInstantProcessOnIO,
                   instant_io_context_, process_id));
  }
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::AddMostVisitedItems(
    const std::vector<InstantMostVisitedItem>& items) {
  most_visited_item_cache_.AddItems(items);

  // Post task to the IO thread to copy the data.
  if (instant_io_context_) {
    std::vector<InstantMostVisitedItemIDPair> items;
    most_visited_item_cache_.GetCurrentItems(&items);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::AddMostVisitedItemsOnIO,
                   instant_io_context_,
                   items));
  }
}

void InstantService::GetCurrentMostVisitedItems(
    std::vector<InstantMostVisitedItemIDPair>* items) const {
  most_visited_item_cache_.GetCurrentItems(items);
}

bool InstantService::GetMostVisitedItemForID(
    InstantRestrictedID most_visited_item_id,
    InstantMostVisitedItem* item) const {
  return most_visited_item_cache_.GetItemWithRestrictedID(
      most_visited_item_id, item);
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::ClearInstantProcessesOnIO,
                   instant_io_context_));
  }
  instant_io_context_ = NULL;
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      int process_id =
          content::Source<content::RenderProcessHost>(source)->GetID();
      process_ids_.erase(process_id);

      if (instant_io_context_) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&InstantIOContext::RemoveInstantProcessOnIO,
                       instant_io_context_, process_id));
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}
