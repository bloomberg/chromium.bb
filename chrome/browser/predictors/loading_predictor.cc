// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"

namespace predictors {

LoadingPredictor::LoadingPredictor(const LoadingPredictorConfig& config,
                                   Profile* profile) {
  resource_prefetch_predictor_ =
      base::MakeUnique<ResourcePrefetchPredictor>(config, profile);
}

LoadingPredictor::~LoadingPredictor() = default;

void LoadingPredictor::PrepareForPageLoad(const GURL& url, HintOrigin origin) {
  resource_prefetch_predictor_->StartPrefetching(url, origin);
}

void LoadingPredictor::CancelPageLoadHint(const GURL& url) {
  resource_prefetch_predictor_->StopPrefetching(url);
}

void LoadingPredictor::StartInitialization() {
  resource_prefetch_predictor_->StartInitialization();
}

ResourcePrefetchPredictor* LoadingPredictor::resource_prefetch_predictor()
    const {
  return resource_prefetch_predictor_.get();
}

void LoadingPredictor::Shutdown() {
  resource_prefetch_predictor_->Shutdown();
}

}  // namespace predictors
