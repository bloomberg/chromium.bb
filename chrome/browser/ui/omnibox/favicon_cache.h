// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_FAVICON_CACHE_H_
#define CHROME_BROWSER_UI_OMNIBOX_FAVICON_CACHE_H_

#include "base/callback_forward.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_types.h"

namespace gfx {
class Image;
}

class GURL;
class Profile;

typedef base::OnceCallback<void(const gfx::Image& favicon)>
    FaviconFetchedCallback;

// We cache a very small number of favicons so we can synchronously deliver
// them to prevent flicker as the user types.
class FaviconCache {
 public:
  explicit FaviconCache(Profile* profile);
  virtual ~FaviconCache();

  gfx::Image GetFaviconForPageUrl(const GURL& page_url,
                                  FaviconFetchedCallback on_favicon_fetched);

 private:
  void OnFaviconFetched(const GURL& page_url,
                        FaviconFetchedCallback on_favicon_fetched,
                        const favicon_base::FaviconImageResult& result);

  base::CancelableTaskTracker task_tracker_;
  base::MRUCache<GURL, gfx::Image> mru_cache_;
  Profile* profile_;
  base::WeakPtrFactory<FaviconCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_FAVICON_CACHE_H_
