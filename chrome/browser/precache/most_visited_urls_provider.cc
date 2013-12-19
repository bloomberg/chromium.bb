// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/precache/most_visited_urls_provider.h"

#include <list>

#include "base/bind.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "url/gurl.h"

using history::MostVisitedURLList;

namespace {

void OnMostVisitedURLsReceived(
    const precache::URLListProvider::GetURLsCallback& callback,
    const MostVisitedURLList& most_visited_urls) {
  std::list<GURL> urls;
  for (MostVisitedURLList::const_iterator it = most_visited_urls.begin();
       it != most_visited_urls.end(); ++it) {
    if (it->url.SchemeIs("http")) {
      urls.push_back(it->url);
    }
  }
  callback.Run(urls);
}

}  // namespace

namespace precache {

MostVisitedURLsProvider::MostVisitedURLsProvider(history::TopSites* top_sites)
    : top_sites_(top_sites) {}

MostVisitedURLsProvider::~MostVisitedURLsProvider() {}

void MostVisitedURLsProvider::GetURLs(const GetURLsCallback& callback) {
  top_sites_->GetMostVisitedURLs(
      base::Bind(&OnMostVisitedURLsReceived, callback), false);
}

}  // namespace precache
