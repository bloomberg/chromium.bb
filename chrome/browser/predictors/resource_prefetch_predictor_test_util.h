// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_

#include <string>
#include <vector>

#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "components/sessions/core/session_id.h"

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

void InitializePrecacheResource(precache::PrecacheResource* resource,
                                const std::string& url,
                                double weight_ratio,
                                precache::PrecacheResource::Type type);

void InitializeOriginStat(OriginStat* origin_stat,
                          const std::string& origin,
                          int number_of_hits,
                          int number_of_misses,
                          int consecutive_misses,
                          double average_position,
                          bool always_access_network,
                          bool accessed_network);

void InitializeExperiment(precache::PrecacheManifest* manifest,
                          uint32_t experiment_id,
                          const std::vector<bool>& bitset);

PrefetchData CreatePrefetchData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);
RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);
precache::PrecacheManifest CreateManifestData(int64_t id = 0);
OriginData CreateOriginData(const std::string& host,
                            uint64_t last_visit_time = 0);

NavigationID CreateNavigationID(SessionID::id_type tab_id,
                                const std::string& main_frame_url);

ResourcePrefetchPredictor::PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<ResourcePrefetchPredictor::URLRequestSummary>&
        subresource_requests);

ResourcePrefetchPredictor::URLRequestSummary CreateURLRequestSummary(
    SessionID::id_type tab_id,
    const std::string& main_frame_url,
    const std::string& resource_url = std::string(),
    content::ResourceType resource_type = content::RESOURCE_TYPE_MAIN_FRAME,
    net::RequestPriority priority = net::MEDIUM,
    const std::string& mime_type = std::string(),
    bool was_cached = false,
    const std::string& redirect_url = std::string(),
    bool has_validators = false,
    bool always_revalidate = false);

// For printing failures nicely.
std::ostream& operator<<(std::ostream& stream, const PrefetchData& data);
std::ostream& operator<<(std::ostream& stream, const ResourceData& resource);
std::ostream& operator<<(std::ostream& stream, const RedirectData& data);
std::ostream& operator<<(std::ostream& stream, const RedirectStat& redirect);
std::ostream& operator<<(
    std::ostream& stream,
    const ResourcePrefetchPredictor::PageRequestSummary& summary);
std::ostream& operator<<(
    std::ostream& stream,
    const ResourcePrefetchPredictor::URLRequestSummary& summary);
std::ostream& operator<<(std::ostream& stream, const NavigationID& id);

std::ostream& operator<<(std::ostream& os, const OriginData& data);
std::ostream& operator<<(std::ostream& os, const OriginStat& redirect);

bool operator==(const PrefetchData& lhs, const PrefetchData& rhs);
bool operator==(const ResourceData& lhs, const ResourceData& rhs);
bool operator==(const RedirectData& lhs, const RedirectData& rhs);
bool operator==(const RedirectStat& lhs, const RedirectStat& rhs);
bool operator==(const ResourcePrefetchPredictor::PageRequestSummary& lhs,
                const ResourcePrefetchPredictor::PageRequestSummary& rhs);
bool operator==(const ResourcePrefetchPredictor::URLRequestSummary& lhs,
                const ResourcePrefetchPredictor::URLRequestSummary& rhs);
bool operator==(const OriginData& lhs, const OriginData& rhs);
bool operator==(const OriginStat& lhs, const OriginStat& rhs);

}  // namespace predictors

namespace precache {

std::ostream& operator<<(std::ostream& stream,
                         const PrecacheManifest& manifest);
std::ostream& operator<<(std::ostream& stream,
                         const PrecacheResource& resource);

bool operator==(const PrecacheManifest& lhs, const PrecacheManifest& rhs);
bool operator==(const PrecacheResource& lhs, const PrecacheResource& rhs);

}  // namespace precache

#endif  // CHROME_BROWSER_PREDICTORS_RESOURCE_PREFETCH_PREDICTOR_TEST_UTIL_H_
