// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner_factory.h"

#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

namespace offline_pages {

class OfflinerPolicy;

PrerenderingOfflinerFactory::PrerenderingOfflinerFactory(
    content::BrowserContext* context) {
  offliner_ = nullptr;
  context_ = context;
}

PrerenderingOfflinerFactory::~PrerenderingOfflinerFactory() {
  delete offliner_;
}

// static
Offliner* PrerenderingOfflinerFactory::GetOffliner(
    const OfflinerPolicy* policy) {
  // TODO(petewil): Think about whether there might be any threading
  // issues.  This should always happen on the same thread, but make sure.

  // Build a prerendering offliner if we don't already have one cached.
  if (offliner_ == nullptr) {
    // TODO(petewil): Create a PrerenderManager, use for 2nd arg below.

    // Get a pointer to the (unowned) offline page model.
    OfflinePageModel* model =
        OfflinePageModelFactory::GetInstance()->GetForBrowserContext(context_);
    // Ensure we do have a model for saving a page if we prerender it.
    DCHECK(model) << "No OfflinePageModel for offliner";
    offliner_ = new PrerenderingOffliner(context_, policy, model);
  }

  return offliner_;
}

}  // namespace offline_pages
