// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_tracker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace prerender {

PrerenderTracker::PrerenderTracker() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

PrerenderTracker::~PrerenderTracker() {
}

scoped_refptr<PrerenderCookieStore>
PrerenderTracker::GetPrerenderCookieStoreForRenderProcess(
    int process_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  PrerenderCookieStoreMap::const_iterator it =
      prerender_cookie_store_map_.find(process_id);

  if (it == prerender_cookie_store_map_.end())
    return NULL;

  return it->second;
}

void PrerenderTracker::OnCookieChangedForURL(
    int process_id,
    net::CookieMonster* cookie_monster,
    const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // We only care about cookie changes by non-prerender tabs, since only those
  // get applied to the underlying cookie store. Therefore, if a cookie change
  // originated from a prerender, there is nothing to do.
  if (ContainsKey(prerender_cookie_store_map_, process_id))
    return;

  // Since the cookie change did not come from a prerender, broadcast it too
  // all prerenders so that they can be cancelled if there is a conflict.
  for (PrerenderCookieStoreMap::iterator it =
           prerender_cookie_store_map_.begin();
       it != prerender_cookie_store_map_.end();
       ++it) {
    it->second->OnCookieChangedForURL(cookie_monster, url);
  }
}

void PrerenderTracker::RemovePrerenderCookieStoreOnIOThread(int process_id,
                                                            bool was_swapped) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  PrerenderCookieStoreMap::iterator it =
      prerender_cookie_store_map_.find(process_id);

  if (it == prerender_cookie_store_map_.end())
    return;

  std::vector<GURL> cookie_change_urls;
  if (was_swapped)
    it->second->ApplyChanges(&cookie_change_urls);

  scoped_refptr<net::CookieMonster> cookie_monster(
      it->second->default_cookie_monster());

  prerender_cookie_store_map_.erase(it);

  // For each cookie updated by ApplyChanges, we need to call
  // OnCookieChangedForURL so that any potentially conflicting prerenders
  // will be aborted.
  for (std::vector<GURL>::const_iterator url_it = cookie_change_urls.begin();
       url_it != cookie_change_urls.end();
       ++url_it) {
    OnCookieChangedForURL(process_id, cookie_monster.get(), *url_it);
  }
}

void PrerenderTracker::AddPrerenderCookieStoreOnIOThread(
    int process_id,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    const base::Closure& cookie_conflict_cb) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(request_context.get() != NULL);
  net::CookieMonster* cookie_monster =
      request_context->GetURLRequestContext()->cookie_store()->
      GetCookieMonster();
  DCHECK(cookie_monster != NULL);
  bool exists = (prerender_cookie_store_map_.find(process_id) !=
                 prerender_cookie_store_map_.end());
  DCHECK(!exists);
  if (exists)
    return;
  prerender_cookie_store_map_[process_id] =
      new PrerenderCookieStore(make_scoped_refptr(cookie_monster),
                               cookie_conflict_cb);
}

}  // namespace prerender
