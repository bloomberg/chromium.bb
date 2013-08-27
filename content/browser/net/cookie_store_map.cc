// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/cookie_store_map.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_checker.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "url/gurl.h"

namespace {

class CookieClearBarrier {
 public:
  CookieClearBarrier(int n, const base::Closure& done)
      : left_(n),
        all_done_(done) {
  }

  void OnCookieCleared(int num_deleted) {
    DCHECK(thread_checker_.CalledOnValidThread());

    // We don't care how many are deleted.
    DCHECK_GT(left_, 0);

    if (--left_ == 0) {
      all_done_.Run();
      all_done_.Reset();
    }
  }

  static base::Callback<void(int)> NewBarrierClosure(
      int n, const base::Closure& done) {
    DCHECK_GT(n, 0);
    CookieClearBarrier* barrier = new CookieClearBarrier(n, done);
    return base::Bind(&CookieClearBarrier::OnCookieCleared,
                      base::Owned(barrier));
  }

 private:
  int left_;
  base::Closure all_done_;
  base::ThreadChecker thread_checker_;
};

}  // namespace

namespace content {

CookieStoreMap::CookieStoreMap() {
}

CookieStoreMap::~CookieStoreMap() {
}

net::CookieStore* CookieStoreMap::GetForScheme(
    const std::string& scheme) const {
  MapType::const_iterator it = scheme_map_.find(scheme);
  if (it == scheme_map_.end())
    return NULL;
  return it->second;
}

void CookieStoreMap::SetForScheme(const std::string& scheme,
                                  net::CookieStore* cookie_store) {
  DCHECK(scheme_map_.find(scheme) == scheme_map_.end());
  DCHECK(cookie_store);
  scheme_map_[scheme] = cookie_store;
}

void CookieStoreMap::DeleteCookies(const GURL& origin,
                                   const base::Time begin,
                                   const base::Time end,
                                   const base::Closure& done) {
  if (origin.is_empty()) {
    net::CookieStore::DeleteCallback on_delete =
        CookieClearBarrier::NewBarrierClosure(scheme_map_.size(), done);
    for (MapType::const_iterator it = scheme_map_.begin();
         it != scheme_map_.end(); ++it) {
      it->second->DeleteAllCreatedBetweenAsync(begin, end, on_delete);
    }
  } else {
    net::CookieStore* cookie_store = GetForScheme(origin.scheme());
    if (!cookie_store) {
      done.Run();
      return;
    }
    cookie_store->GetCookieMonster()->DeleteAllCreatedBetweenForHostAsync(
        begin, end, origin, CookieClearBarrier::NewBarrierClosure(1, done));
  }
}

CookieStoreMap* CookieStoreMap::Clone() const {
  CookieStoreMap* cloned_map = new CookieStoreMap();
  cloned_map->scheme_map_ = scheme_map_;
  return cloned_map;
}

}  // namespace content
