// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

namespace predictors {

ResourceData CreateResourceData(const std::string& resource_url,
                                content::ResourceType resource_type,
                                int number_of_hits,
                                int number_of_misses,
                                int consecutive_misses,
                                double average_position,
                                net::RequestPriority priority,
                                bool has_validators,
                                bool always_revalidate);

// For printing failures nicely.
void PrintTo(const ResourceData& resource, ::std::ostream* os);
void PrintTo(const ResourcePrefetchPredictorTables::PrefetchData& data,
             ::std::ostream* os);

bool operator==(const ResourceData& lhs, const ResourceData& rhs);
bool operator==(const ResourcePrefetchPredictorTables::PrefetchData& lhs,
                const ResourcePrefetchPredictorTables::PrefetchData& rhs);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_
