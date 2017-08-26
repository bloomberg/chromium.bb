// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PAGE_LOAD_STATISTICS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PAGE_LOAD_STATISTICS_H_

#include "base/macros.h"
#include "components/subresource_filter/core/common/activation_state.h"
#include "components/subresource_filter/core/common/document_load_statistics.h"

namespace subresource_filter {

// This class is notified of metrics recorded for individual (sub-)documents of
// a page, aggregates them, and logs the aggregated metrics to UMA histograms
// when the page load is complete (at the load event).
//
// Additionally, it manages aggregation of page load metrics like the number of
// popups blocked.
class PageLoadStatistics {
 public:
  PageLoadStatistics(const ActivationState& state);
  ~PageLoadStatistics();

  void OnDocumentLoadStatistics(const DocumentLoadStatistics& statistics);
  void OnDidFinishLoad();

  void OnBlockedPopup();

 private:
  ActivationState activation_state_;

  // Statistics about subresource loads, aggregated across all frames of the
  // current page.
  DocumentLoadStatistics aggregated_document_statistics_;

  int num_popups_blocked_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PageLoadStatistics);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_PAGE_LOAD_STATISTICS_H_
