// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_saver_site_breakdown_metrics_observer.h"

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

DataSaverSiteBreakdownMetricsObserver::DataSaverSiteBreakdownMetricsObserver() =
    default;

DataSaverSiteBreakdownMetricsObserver::
    ~DataSaverSiteBreakdownMetricsObserver() = default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataSaverSiteBreakdownMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!data_reduction_proxy::params::IsDataSaverSiteBreakdownUsingPLMEnabled())
    return STOP_OBSERVING;

  // This BrowserContext is valid for the lifetime of
  // DataReductionProxyMetricsObserver. BrowserContext is always valid and
  // non-nullptr in NavigationControllerImpl, which is a member of WebContents.
  // A raw pointer to BrowserContext taken at this point will be valid until
  // after WebContent's destructor. The latest that PageLoadTracker's destructor
  // will be called is in MetricsWebContentsObserver's destructor, which is
  // called in WebContents destructor.
  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();
  committed_host_ = navigation_handle->GetURL().HostNoBrackets();
  return CONTINUE_OBSERVING;
}

void DataSaverSiteBreakdownMetricsObserver::OnDataUseObserved(
    int64_t received_data_length,
    int64_t data_reduction_proxy_bytes_saved) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_reduction_proxy::DataReductionProxySettings*
      data_reduction_proxy_settings =
          DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
              browser_context_);
  if (data_reduction_proxy_settings &&
      data_reduction_proxy_settings->data_reduction_proxy_service()) {
    DCHECK(!committed_host_.empty());
    data_reduction_proxy_settings->data_reduction_proxy_service()
        ->UpdateDataUseForHost(
            received_data_length,
            received_data_length + data_reduction_proxy_bytes_saved,
            committed_host_);
  }
}
