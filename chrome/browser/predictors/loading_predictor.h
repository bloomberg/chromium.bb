// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/predictors/resource_prefetch_common.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace predictors {

class ResourcePrefetchPredictor;

// Entry point for the Loading predictor.
// From a high-level request (GURL and motivation) and a database of historical
// data, initiates predictive actions to speed up page loads.
//
// See ResourcePrefetchPredictor for a description of the resource prefetch
// predictor.
//
// All methods must be called from the UI thread.
class LoadingPredictor : public KeyedService,
                         public base::SupportsWeakPtr<LoadingPredictor> {
 public:
  LoadingPredictor(const LoadingPredictorConfig& config, Profile* profile);
  ~LoadingPredictor() override;

  // Hints that a page load is expected for |url|, with the hint coming from a
  // given |origin|. May trigger actions, such as prefetch and/or preconnect.
  void PrepareForPageLoad(const GURL& url, HintOrigin origin);

  // Indicates that a page load hint is no longer active.
  void CancelPageLoadHint(const GURL& url);

  // Starts initialization, will complete asynchronously.
  void StartInitialization();

  // Don't use, internal only.
  ResourcePrefetchPredictor* resource_prefetch_predictor() const;

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<ResourcePrefetchPredictor> resource_prefetch_predictor_;

  DISALLOW_COPY_AND_ASSIGN(LoadingPredictor);
};

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_PREDICTOR_H_
