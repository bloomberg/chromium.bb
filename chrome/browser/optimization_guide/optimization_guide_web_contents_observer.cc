// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/hints_fetcher.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"

namespace {

// Records if the host for the current navigation was successfully
// covered by a HintsFetch. HintsFetching must be enabled and only HTTPS
// navigations are logged.
void MaybeRecordHintsFetcherCoverage(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetURL().SchemeIs(url::kHttpsScheme))
    return;
  if (!optimization_guide::features::IsHintsFetchingEnabled())
    return;

  optimization_guide::HintsFetcher::RecordHintsFetcherCoverage(
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs(),
      navigation_handle->GetURL().GetOrigin().host());
}

}  // namespace

OptimizationGuideWebContentsObserver::OptimizationGuideWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  optimization_guide_keyed_service_ =
      OptimizationGuideKeyedServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

OptimizationGuideWebContentsObserver::~OptimizationGuideWebContentsObserver() =
    default;

OptimizationGuideNavigationData* OptimizationGuideWebContentsObserver::
    GetOrCreateOptimizationGuideNavigationData(
        content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int64_t navigation_id = navigation_handle->GetNavigationId();
  if (inflight_optimization_guide_navigation_datas_.find(navigation_id) ==
      inflight_optimization_guide_navigation_datas_.end()) {
    // We do not have one already - create one.
    inflight_optimization_guide_navigation_datas_.emplace(
        std::piecewise_construct, std::forward_as_tuple(navigation_id),
        std::forward_as_tuple(navigation_id));
  }

  DCHECK(inflight_optimization_guide_navigation_datas_.find(navigation_id) !=
         inflight_optimization_guide_navigation_datas_.end());
  return &(inflight_optimization_guide_navigation_datas_.find(navigation_id)
               ->second);
}

void OptimizationGuideWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInMainFrame())
    return;

  // Record the HintsFetcher coverage for the navigation, regardless if the
  // keyed service is active or not.
  MaybeRecordHintsFetcherCoverage(navigation_handle);

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->MaybeLoadHintForNavigation(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!navigation_handle->IsInMainFrame())
    return;

  // Record the HintsFetcher coverage for the navigation, regardless if the
  // keyed service is active or not.
  MaybeRecordHintsFetcherCoverage(navigation_handle);

  if (!optimization_guide_keyed_service_)
    return;

  optimization_guide_keyed_service_->MaybeLoadHintForNavigation(
      navigation_handle);
}

void OptimizationGuideWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Delete Optimization Guide information later, so that other
  // DidFinishNavigation methods can reliably use
  // GetOptimizationGuideNavigationData regardless of order of
  // WebContentsObservers.
  // Note that a lot of Navigations (sub-frames, same document, non-committed,
  // etc.) might not have navigation data associated with them, but we reduce
  // likelihood of future leaks by always trying to remove the data.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OptimizationGuideWebContentsObserver::
                         FlushMetricsAndRemoveOptimizationGuideNavigationData,
                     weak_factory_.GetWeakPtr(),
                     navigation_handle->GetNavigationId(),
                     navigation_handle->HasCommitted()));
}

void OptimizationGuideWebContentsObserver::
    FlushMetricsAndRemoveOptimizationGuideNavigationData(int64_t navigation_id,
                                                         bool has_committed) {
  auto nav_data_iter =
      inflight_optimization_guide_navigation_datas_.find(navigation_id);
  if (nav_data_iter == inflight_optimization_guide_navigation_datas_.end())
    return;

  (nav_data_iter->second).RecordMetrics(has_committed);

  inflight_optimization_guide_navigation_datas_.erase(navigation_id);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OptimizationGuideWebContentsObserver)
