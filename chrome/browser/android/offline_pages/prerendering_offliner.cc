// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

namespace offline_pages {

PrerenderingOffliner::PrerenderingOffliner(PrerenderManager* prerender_manager,
                                           OfflinePageModel* offline_page_model)
    : loader_(new PrerenderingLoader(prerender_manager)) {}

PrerenderingOffliner::~PrerenderingOffliner() {}

bool PrerenderingOffliner::LoadAndSave(const SavePageRequest& request,
                                       const CompletionCallback& callback) {
  // TODO(dougarnett): implement.
  return false;
}

void PrerenderingOffliner::Cancel() {
  loader_->StopLoading();
}

}  // namespace offline_pages
