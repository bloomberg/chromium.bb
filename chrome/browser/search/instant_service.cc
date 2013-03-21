// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_factory.h"
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

namespace {

// Copies deleted urls out of the history data structure |details| into a
// straight vector of GURLs.
void HistoryDetailsToDeletedURLs(const history::URLsDeletedDetails& details,
                                 std::vector<GURL>* deleted_urls) {
  for (history::URLRows::const_iterator it = details.rows.begin();
       it != details.rows.end();
       ++it) {
    deleted_urls->push_back(it->url());
  }
}

}  // namespace

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      last_most_visited_item_id_(0) {
  // Stub for unit tests.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                 content::NotificationService::AllSources());

  instant_io_context_ = new InstantIOContext();

  if (profile_ && profile_->GetResourceContext()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::SetUserDataOnIO,
                   profile->GetResourceContext(), instant_io_context_));
  }

/* TODO(dhollowa): Temporarily disabling to diagnose perf regression.
   http://crbug.com/189163.

  // Set up the data sources that Instant uses on the NTP.
#if defined(ENABLE_THEMES)
  content::URLDataSource::Add(profile, new ThemeSource(profile));
#endif
  content::URLDataSource::Add(profile, new ThumbnailSource(profile));
  content::URLDataSource::Add(profile, new FaviconSource(
      profile, FaviconSource::FAVICON));
*/
  content::URLDataSource::Add(profile, new LocalOmniboxPopupSource());
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

  uint64 most_visited_item_id = 0;
  if (base::StringToUint64(path, &most_visited_item_id)) {
    GURL url;
    if (instant_service->GetURLForMostVisitedItemId(most_visited_item_id, &url))
      return url.spec();
  }
  return path;
}

const std::string InstantService::MaybeTranslateInstantPathOnIO(
    const net::URLRequest* request, const std::string& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  uint64 most_visited_item_id = 0;
  if (base::StringToUint64(path, &most_visited_item_id)) {
    GURL url;
    if (InstantIOContext::GetURLForMostVisitedItemId(request,
                                                     most_visited_item_id,
                                                     &url))
      return url.spec();
  }
  return path;
}

// static
bool InstantService::IsInstantPath(const GURL& url) {
  // Strip leading slash.
  std::string path = url.path().substr(1);

  // Check that path is of Most Visited item ID form.
  uint64 dummy = 0;
  return base::StringToUint64(path, &dummy);
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

uint64 InstantService::AddURL(const GURL& url) {
  uint64 id = 0;
  if (GetMostVisitedItemIDForURL(url, &id))
    return id;

  last_most_visited_item_id_++;
  most_visited_item_id_to_url_map_[last_most_visited_item_id_] = url;
  url_to_most_visited_item_id_map_[url] = last_most_visited_item_id_;

  if (instant_io_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::AddMostVisitedItemIDOnIO,
                   instant_io_context_, last_most_visited_item_id_, url));
  }

  return last_most_visited_item_id_;
}

bool InstantService::GetMostVisitedItemIDForURL(
    const GURL& url,
    uint64 *most_visited_item_id) {
  std::map<GURL, uint64>::iterator it =
      url_to_most_visited_item_id_map_.find(url);
  if (it != url_to_most_visited_item_id_map_.end()) {
    *most_visited_item_id = it->second;
    return true;
  }
  *most_visited_item_id = 0;
  return false;
}

bool InstantService::GetURLForMostVisitedItemId(uint64 most_visited_item_id,
                                                GURL* url) {
  std::map<uint64, GURL>::iterator it =
      most_visited_item_id_to_url_map_.find(most_visited_item_id);
  if (it != most_visited_item_id_to_url_map_.end()) {
    *url = it->second;
    return true;
  }
  *url = GURL();
  return false;
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
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED: {
      content::Details<history::URLsDeletedDetails> det(details);
      std::vector<GURL> deleted_urls;
      HistoryDetailsToDeletedURLs(*det.ptr(), &deleted_urls);

      std::vector<uint64> deleted_ids;
      if (det->all_history) {
        url_to_most_visited_item_id_map_.clear();
        most_visited_item_id_to_url_map_.clear();
      } else {
        DeleteHistoryURLs(deleted_urls, &deleted_ids);
      }

      if (instant_io_context_) {
        BrowserThread::PostTask(
            BrowserThread::IO, FROM_HERE,
            base::Bind(&InstantIOContext::DeleteMostVisitedURLsOnIO,
                       instant_io_context_, deleted_ids, det->all_history));
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::DeleteHistoryURLs(const std::vector<GURL>& deleted_urls,
                                       std::vector<uint64>* deleted_ids) {
  for (std::vector<GURL>::const_iterator it = deleted_urls.begin();
       it != deleted_urls.end();
       ++it) {
    std::map<GURL, uint64>::iterator item =
        url_to_most_visited_item_id_map_.find(*it);
    if (item != url_to_most_visited_item_id_map_.end()) {
      uint64 most_visited_item_id = item->second;
      url_to_most_visited_item_id_map_.erase(item);
      most_visited_item_id_to_url_map_.erase(
          most_visited_item_id_to_url_map_.find(most_visited_item_id));
      deleted_ids->push_back(most_visited_item_id);
    }
  }
}
