// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_

#include <string>

#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"

namespace predictors {

void InitializeResourceData(ResourceData* resource,
                            const std::string& resource_url,
                            content::ResourceType resource_type,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses,
                            double average_position,
                            net::RequestPriority priority,
                            bool has_validators,
                            bool always_revalidate);

void InitializeRedirectStat(RedirectStat* redirect,
                            const std::string& url,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses);

PrefetchData CreatePrefetchData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);
RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);

// For printing failures nicely.
void PrintTo(const PrefetchData& data, ::std::ostream* os);
void PrintTo(const ResourceData& resource, ::std::ostream* os);
void PrintTo(const RedirectData& data, ::std::ostream* os);
void PrintTo(const RedirectStat& redirect, ::std::ostream* os);

bool operator==(const PrefetchData& lhs, const PrefetchData& rhs);
bool operator==(const ResourceData& lhs, const ResourceData& rhs);
bool operator==(const RedirectData& lhs, const RedirectData& rhs);
bool operator==(const RedirectStat& lhs, const RedirectStat& rhs);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_
