// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/background_loader_offliner_factory.h"

#include "chrome/browser/android/offline_pages/background_loader_offliner.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"

namespace offline_pages {

class OfflinerPolicy;

BackgroundLoaderOfflinerFactory::BackgroundLoaderOfflinerFactory(
    content::BrowserContext* context) {
  offliner_ = nullptr;
  context_ = context;
}

BackgroundLoaderOfflinerFactory::~BackgroundLoaderOfflinerFactory() {
  delete offliner_;
}

Offliner* BackgroundLoaderOfflinerFactory::GetOffliner(
    const OfflinerPolicy* policy) {
  if (offliner_ == nullptr) {
    OfflinePageModel* model =
        OfflinePageModelFactory::GetInstance()->GetForBrowserContext(context_);

    offliner_ = new BackgroundLoaderOffliner(context_, policy, model);
  }

  return offliner_;
}

} // namespace offline_pages
