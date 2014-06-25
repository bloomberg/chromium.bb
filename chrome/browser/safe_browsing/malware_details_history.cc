// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the MalwareDetailsRedirectsCollector class.

#include "chrome/browser/safe_browsing/malware_details_history.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/malware_details.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserThread;

MalwareDetailsRedirectsCollector::MalwareDetailsRedirectsCollector(
    Profile* profile)
    : profile_(profile),
      has_started_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::Source<Profile>(profile));
  }
}

void MalwareDetailsRedirectsCollector::StartHistoryCollection(
    const std::vector<GURL>& urls,
    const base::Closure& callback) {
  DVLOG(1) << "Num of urls to check in history service: " << urls.size();
  has_started_ = true;
  callback_ = callback;

  if (urls.size() == 0) {
    AllDone();
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MalwareDetailsRedirectsCollector::StartGetRedirects,
                 this, urls));
}

bool MalwareDetailsRedirectsCollector::HasStarted() const {
  return has_started_;
}

const std::vector<safe_browsing::RedirectChain>&
MalwareDetailsRedirectsCollector::GetCollectedUrls() const {
  return redirects_urls_;
}

void MalwareDetailsRedirectsCollector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, chrome::NOTIFICATION_PROFILE_DESTROYED);
  DVLOG(1) << "Profile gone.";
  profile_ = NULL;
}

MalwareDetailsRedirectsCollector::~MalwareDetailsRedirectsCollector() {}

void MalwareDetailsRedirectsCollector::StartGetRedirects(
        const std::vector<GURL>& urls) {
  // History access from profile needs to happen in UI thread
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (size_t i = 0; i < urls.size(); ++i) {
    urls_.push_back(urls[i]);
  }
  urls_it_ = urls_.begin();
  GetRedirects(*urls_it_);
}

void MalwareDetailsRedirectsCollector::GetRedirects(const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!profile_) {
    AllDone();
    return;
  }

  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (!history) {
    AllDone();
    return;
  }

  history->QueryRedirectsTo(
      url,
      base::Bind(&MalwareDetailsRedirectsCollector::OnGotQueryRedirectsTo,
                 base::Unretained(this),
                 url),
      &request_tracker_);
}

void MalwareDetailsRedirectsCollector::OnGotQueryRedirectsTo(
    const GURL& url,
    const history::RedirectList* redirect_list) {
  if (!redirect_list->empty()) {
    std::vector<GURL> urllist;
    urllist.push_back(url);
    urllist.insert(urllist.end(), redirect_list->begin(), redirect_list->end());
    redirects_urls_.push_back(urllist);
  }

  // Proceed to next url
  ++urls_it_;

  if (urls_it_ == urls_.end()) {
    AllDone();
    return;
  }

  GetRedirects(*urls_it_);
}

void MalwareDetailsRedirectsCollector::AllDone() {
  DVLOG(1) << "AllDone";
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, callback_);
  callback_.Reset();
}
