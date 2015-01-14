// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_

#include <map>
#include <set>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/prerender/prerender_cookie_store.h"
#include "content/public/browser/render_process_host_observer.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace prerender {

// Global object for maintaining various prerender state on the IO thread.
class PrerenderTracker {
 public:
  typedef std::pair<int, int> ChildRouteIdPair;

  PrerenderTracker();
  virtual ~PrerenderTracker();

  // Gets the Prerender Cookie Store for a specific render process, if it
  // is a prerender. Only to be called from the IO thread.
  scoped_refptr<PrerenderCookieStore> GetPrerenderCookieStoreForRenderProcess(
      int process_id);

  // Called when a given render process has changed a cookie for |url|,
  // in |cookie_monster|.
  // Only to be called from the IO thread.
  void OnCookieChangedForURL(int process_id,
                             net::CookieMonster* cookie_monster,
                             const GURL& url);

  void AddPrerenderCookieStoreOnIOThread(
      int process_id,
      scoped_refptr<net::URLRequestContextGetter> request_context,
      const base::Closure& cookie_conflict_cb);

  void RemovePrerenderCookieStoreOnIOThread(int process_id, bool was_swapped);

 private:
  // Map of prerendering render process ids to PrerenderCookieStore used for
  // the prerender. Only to be used on the IO thread.
  typedef base::hash_map<int, scoped_refptr<PrerenderCookieStore> >
      PrerenderCookieStoreMap;
  PrerenderCookieStoreMap prerender_cookie_store_map_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderTracker);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_TRACKER_H_
