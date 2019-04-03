// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace {
const char kAppListSearchResultOpenTypeHistogram[] =
    "Apps.AppListSearchResultOpenTypeV2";
}

namespace app_list {

void RecordSearchResultOpenTypeHistogram(SearchResultType type) {
  if (type == SEARCH_RESULT_TYPE_BOUNDARY) {
    NOTREACHED();
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kAppListSearchResultOpenTypeHistogram, type,
                            SEARCH_RESULT_TYPE_BOUNDARY);
}

}  // namespace app_list
