// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_test_util.h"

#include <cmath>
#include <memory>
#include <utility>

#include "content/public/browser/resource_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_test_util.h"

namespace {

class EmptyURLRequestDelegate : public net::URLRequest::Delegate {
  void OnResponseStarted(net::URLRequest* request, int net_error) override {}
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override {}
};

EmptyURLRequestDelegate g_empty_url_request_delegate;

bool AlmostEqual(const double x, const double y) {
  return std::fabs(x - y) <= 1e-6;  // Arbitrary but close enough.
}

}  // namespace

namespace predictors {

using Prediction = ResourcePrefetchPredictor::Prediction;

MockResourcePrefetchPredictor::MockResourcePrefetchPredictor(
    const LoadingPredictorConfig& config,
    Profile* profile)
    : ResourcePrefetchPredictor(config, profile) {}

MockResourcePrefetchPredictor::~MockResourcePrefetchPredictor() = default;

void InitializeResourceData(ResourceData* resource,
                            const std::string& resource_url,
                            content::ResourceType resource_type,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses,
                            double average_position,
                            net::RequestPriority priority,
                            bool has_validators,
                            bool always_revalidate) {
  resource->set_resource_url(resource_url);
  resource->set_resource_type(
      static_cast<ResourceData::ResourceType>(resource_type));
  resource->set_number_of_hits(number_of_hits);
  resource->set_number_of_misses(number_of_misses);
  resource->set_consecutive_misses(consecutive_misses);
  resource->set_average_position(average_position);
  resource->set_priority(static_cast<ResourceData::Priority>(priority));
  resource->set_before_first_contentful_paint(true);
  resource->set_has_validators(has_validators);
  resource->set_always_revalidate(always_revalidate);
}

void InitializeRedirectStat(RedirectStat* redirect,
                            const std::string& url,
                            int number_of_hits,
                            int number_of_misses,
                            int consecutive_misses) {
  redirect->set_url(url);
  redirect->set_number_of_hits(number_of_hits);
  redirect->set_number_of_misses(number_of_misses);
  redirect->set_consecutive_misses(consecutive_misses);
}

void InitializeOriginStat(OriginStat* origin_stat,
                          const std::string& origin,
                          int number_of_hits,
                          int number_of_misses,
                          int consecutive_misses,
                          double average_position,
                          bool always_access_network,
                          bool accessed_network) {
  origin_stat->set_origin(origin);
  origin_stat->set_number_of_hits(number_of_hits);
  origin_stat->set_number_of_misses(number_of_misses);
  origin_stat->set_consecutive_misses(consecutive_misses);
  origin_stat->set_average_position(average_position);
  origin_stat->set_always_access_network(always_access_network);
  origin_stat->set_accessed_network(accessed_network);
}

PrefetchData CreatePrefetchData(const std::string& primary_key,
                                uint64_t last_visit_time) {
  PrefetchData data;
  data.set_primary_key(primary_key);
  data.set_last_visit_time(last_visit_time);
  return data;
}

RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time) {
  RedirectData data;
  data.set_primary_key(primary_key);
  data.set_last_visit_time(last_visit_time);
  return data;
}

OriginData CreateOriginData(const std::string& host, uint64_t last_visit_time) {
  OriginData data;
  data.set_host(host);
  data.set_last_visit_time(last_visit_time);
  return data;
}

NavigationID CreateNavigationID(SessionID::id_type tab_id,
                                const std::string& main_frame_url) {
  NavigationID navigation_id;
  navigation_id.tab_id = tab_id;
  navigation_id.main_frame_url = GURL(main_frame_url);
  navigation_id.creation_time = base::TimeTicks::Now();
  return navigation_id;
}

PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<URLRequestSummary>& subresource_requests) {
  GURL main_frame_gurl(main_frame_url);
  PageRequestSummary summary(main_frame_gurl);
  summary.initial_url = GURL(initial_url);
  summary.subresource_requests = subresource_requests;
  summary.UpdateOrAddToOrigins(CreateURLRequestSummary(1, main_frame_url));
  for (auto& request_summary : subresource_requests)
    summary.UpdateOrAddToOrigins(request_summary);
  return summary;
}

URLRequestSummary CreateURLRequestSummary(SessionID::id_type tab_id,
                                          const std::string& main_frame_url,
                                          const std::string& resource_url,
                                          content::ResourceType resource_type,
                                          net::RequestPriority priority,
                                          const std::string& mime_type,
                                          bool was_cached,
                                          const std::string& redirect_url,
                                          bool has_validators,
                                          bool always_revalidate) {
  URLRequestSummary summary;
  summary.navigation_id = CreateNavigationID(tab_id, main_frame_url);
  summary.resource_url =
      resource_url.empty() ? GURL(main_frame_url) : GURL(resource_url);
  summary.request_url = summary.resource_url;
  summary.resource_type = resource_type;
  summary.priority = priority;
  summary.before_first_contentful_paint = true;
  summary.mime_type = mime_type;
  summary.was_cached = was_cached;
  if (!redirect_url.empty())
    summary.redirect_url = GURL(redirect_url);
  summary.has_validators = has_validators;
  summary.always_revalidate = always_revalidate;
  summary.is_no_store = false;
  summary.network_accessed = true;
  return summary;
}

URLRequestSummary CreateRedirectRequestSummary(
    SessionID::id_type session_id,
    const std::string& main_frame_url,
    const std::string& redirect_url) {
  URLRequestSummary summary =
      CreateURLRequestSummary(session_id, main_frame_url);
  summary.redirect_url = GURL(redirect_url);
  return summary;
}

ResourcePrefetchPredictor::Prediction CreatePrediction(
    const std::string& main_frame_key,
    std::vector<GURL> subresource_urls) {
  Prediction prediction;
  prediction.main_frame_key = main_frame_key;
  prediction.subresource_urls = subresource_urls;
  prediction.is_host = true;
  prediction.is_redirected = false;
  return prediction;
}

PreconnectPrediction CreatePreconnectPrediction(
    std::string host,
    bool is_redirected,
    std::vector<GURL> preconnect_origins,
    std::vector<GURL> preresolve_hosts) {
  PreconnectPrediction prediction;
  prediction.host = host;
  prediction.is_redirected = is_redirected;
  prediction.preconnect_origins = preconnect_origins;
  prediction.preresolve_hosts = preresolve_hosts;
  return prediction;
}

void PopulateTestConfig(LoadingPredictorConfig* config, bool small_db) {
  if (small_db) {
    config->max_urls_to_track = 3;
    config->max_hosts_to_track = 2;
    config->min_url_visit_count = 2;
    config->max_resources_per_entry = 4;
    config->max_consecutive_misses = 2;
    config->max_redirect_consecutive_misses = 2;
    config->min_resource_confidence_to_trigger_prefetch = 0.5;
  }
  config->is_host_learning_enabled = true;
  config->is_url_learning_enabled = true;
  config->is_origin_learning_enabled = true;
  config->mode = LoadingPredictorConfig::LEARNING;
}

scoped_refptr<net::HttpResponseHeaders> MakeResponseHeaders(
    const char* headers) {
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers, strlen(headers)));
}

MockURLRequestJob::MockURLRequestJob(
    net::URLRequest* request,
    const net::HttpResponseInfo& response_info,
    const net::LoadTimingInfo& load_timing_info,
    const std::string& mime_type)
    : net::URLRequestJob(request, nullptr),
      response_info_(response_info),
      load_timing_info_(load_timing_info),
      mime_type_(mime_type) {}

bool MockURLRequestJob::GetMimeType(std::string* mime_type) const {
  *mime_type = mime_type_;
  return true;
}

void MockURLRequestJob::Start() {
  NotifyHeadersComplete();
}

void MockURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  *info = response_info_;
}

void MockURLRequestJob::GetLoadTimingInfo(net::LoadTimingInfo* info) const {
  *info = load_timing_info_;
}

MockURLRequestJobFactory::MockURLRequestJobFactory() {}
MockURLRequestJobFactory::~MockURLRequestJobFactory() {}

void MockURLRequestJobFactory::Reset() {
  response_info_ = net::HttpResponseInfo();
  mime_type_ = std::string();
}

net::URLRequestJob* MockURLRequestJobFactory::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return new MockURLRequestJob(request, response_info_, load_timing_info_,
                               mime_type_);
}

net::URLRequestJob* MockURLRequestJobFactory::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  return nullptr;
}

net::URLRequestJob* MockURLRequestJobFactory::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

bool MockURLRequestJobFactory::IsHandledProtocol(
    const std::string& scheme) const {
  return true;
}

bool MockURLRequestJobFactory::IsSafeRedirectTarget(
    const GURL& location) const {
  return true;
}

std::unique_ptr<net::URLRequest> CreateURLRequest(
    const net::TestURLRequestContext& url_request_context,
    const GURL& url,
    net::RequestPriority priority,
    content::ResourceType resource_type,
    bool is_main_frame) {
  std::unique_ptr<net::URLRequest> request = url_request_context.CreateRequest(
      url, priority, &g_empty_url_request_delegate);
  request->set_site_for_cookies(url);
  content::ResourceRequestInfo::AllocateForTesting(
      request.get(), resource_type, nullptr, -1, -1, -1, is_main_frame, false,
      true, content::PREVIEWS_OFF);
  request->Start();
  return request;
}

std::ostream& operator<<(std::ostream& os, const PrefetchData& data) {
  os << "[" << data.primary_key() << "," << data.last_visit_time() << "]"
     << std::endl;
  for (const ResourceData& resource : data.resources())
    os << "\t\t" << resource << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const ResourceData& resource) {
  return os << "[" << resource.resource_url() << "," << resource.resource_type()
            << "," << resource.number_of_hits() << ","
            << resource.number_of_misses() << ","
            << resource.consecutive_misses() << ","
            << resource.average_position() << "," << resource.priority() << ","
            << resource.before_first_contentful_paint() << ","
            << resource.has_validators() << "," << resource.always_revalidate()
            << "]";
}

std::ostream& operator<<(std::ostream& os, const RedirectData& data) {
  os << "[" << data.primary_key() << "," << data.last_visit_time() << "]"
     << std::endl;
  for (const RedirectStat& redirect : data.redirect_endpoints())
    os << "\t\t" << redirect << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const RedirectStat& redirect) {
  return os << "[" << redirect.url() << "," << redirect.number_of_hits() << ","
            << redirect.number_of_misses() << ","
            << redirect.consecutive_misses() << "]";
}

std::ostream& operator<<(std::ostream& os, const OriginData& data) {
  os << "[" << data.host() << "," << data.last_visit_time() << "]" << std::endl;
  for (const OriginStat& origin : data.origins())
    os << "\t\t" << origin << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const OriginStat& origin) {
  return os << "[" << origin.origin() << "," << origin.number_of_hits() << ","
            << origin.number_of_misses() << "," << origin.consecutive_misses()
            << "," << origin.average_position() << ","
            << origin.always_access_network() << ","
            << origin.accessed_network() << "]";
}

std::ostream& operator<<(std::ostream& os,
                         const OriginRequestSummary& summary) {
  return os << "[" << summary.origin << "," << summary.always_access_network
            << "," << summary.accessed_network << ","
            << summary.first_occurrence << "]";
}

std::ostream& operator<<(std::ostream& os, const PageRequestSummary& summary) {
  os << "[" << summary.main_frame_url << "," << summary.initial_url << "]"
     << std::endl;
  for (const auto& request : summary.subresource_requests)
    os << "\t\t" << request << std::endl;
  for (const auto& pair : summary.origins)
    os << "\t\t" << pair.first << ":" << pair.second << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const URLRequestSummary& summary) {
  return os << "[" << summary.navigation_id << "," << summary.resource_url
            << "," << summary.resource_type << "," << summary.priority << ","
            << summary.before_first_contentful_paint << "," << summary.mime_type
            << "," << summary.was_cached << "," << summary.redirect_url << ","
            << summary.has_validators << "," << summary.always_revalidate
            << "]";
}

std::ostream& operator<<(std::ostream& os, const NavigationID& navigation_id) {
  return os << navigation_id.tab_id << "," << navigation_id.main_frame_url;
}

std::ostream& operator<<(std::ostream& os,
                         const PreconnectPrediction& prediction) {
  os << "[" << prediction.host << "," << prediction.is_redirected << "]"
     << std::endl;

  os << "Preconnect:" << std::endl;
  for (const auto& url : prediction.preconnect_origins)
    os << "\t\t" << url << std::endl;

  os << "Preresolve:" << std::endl;
  for (const auto& url : prediction.preresolve_hosts)
    os << "\t\t" << url << std::endl;
  return os;
}

bool operator==(const PrefetchData& lhs, const PrefetchData& rhs) {
  bool equal = lhs.primary_key() == rhs.primary_key() &&
               lhs.resources_size() == rhs.resources_size();

  if (!equal)
    return false;

  for (int i = 0; i < lhs.resources_size(); ++i)
    equal = equal && lhs.resources(i) == rhs.resources(i);

  return equal;
}

bool operator==(const ResourceData& lhs, const ResourceData& rhs) {
  return lhs.resource_url() == rhs.resource_url() &&
         lhs.resource_type() == rhs.resource_type() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses() &&
         AlmostEqual(lhs.average_position(), rhs.average_position()) &&
         lhs.priority() == rhs.priority() &&
         lhs.before_first_contentful_paint() ==
             rhs.before_first_contentful_paint() &&
         lhs.has_validators() == rhs.has_validators() &&
         lhs.always_revalidate() == rhs.always_revalidate();
}

bool operator==(const RedirectData& lhs, const RedirectData& rhs) {
  bool equal = lhs.primary_key() == rhs.primary_key() &&
               lhs.redirect_endpoints_size() == rhs.redirect_endpoints_size();

  if (!equal)
    return false;

  for (int i = 0; i < lhs.redirect_endpoints_size(); ++i)
    equal = equal && lhs.redirect_endpoints(i) == rhs.redirect_endpoints(i);

  return equal;
}

bool operator==(const RedirectStat& lhs, const RedirectStat& rhs) {
  return lhs.url() == rhs.url() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses();
}

bool operator==(const PageRequestSummary& lhs, const PageRequestSummary& rhs) {
  return lhs.main_frame_url == rhs.main_frame_url &&
         lhs.initial_url == rhs.initial_url &&
         lhs.subresource_requests == rhs.subresource_requests &&
         lhs.origins == rhs.origins;
}

bool operator==(const URLRequestSummary& lhs, const URLRequestSummary& rhs) {
  return lhs.navigation_id == rhs.navigation_id &&
         lhs.resource_url == rhs.resource_url &&
         lhs.resource_type == rhs.resource_type &&
         lhs.priority == rhs.priority && lhs.mime_type == rhs.mime_type &&
         lhs.before_first_contentful_paint ==
             rhs.before_first_contentful_paint &&
         lhs.was_cached == rhs.was_cached &&
         lhs.redirect_url == rhs.redirect_url &&
         lhs.has_validators == rhs.has_validators &&
         lhs.always_revalidate == rhs.always_revalidate;
}

bool operator==(const OriginRequestSummary& lhs,
                const OriginRequestSummary& rhs) {
  return lhs.origin == rhs.origin &&
         lhs.always_access_network == rhs.always_access_network &&
         lhs.accessed_network == rhs.accessed_network &&
         lhs.first_occurrence == rhs.first_occurrence;
}

bool operator==(const OriginData& lhs, const OriginData& rhs) {
  bool equal =
      lhs.host() == rhs.host() && lhs.origins_size() == rhs.origins_size();
  if (!equal)
    return false;

  for (int i = 0; i < lhs.origins_size(); ++i)
    equal = equal && lhs.origins(i) == rhs.origins(i);

  return equal;
}

bool operator==(const OriginStat& lhs, const OriginStat& rhs) {
  return lhs.origin() == rhs.origin() &&
         lhs.number_of_hits() == rhs.number_of_hits() &&
         lhs.number_of_misses() == rhs.number_of_misses() &&
         lhs.consecutive_misses() == rhs.consecutive_misses() &&
         AlmostEqual(lhs.average_position(), rhs.average_position()) &&
         lhs.always_access_network() == rhs.always_access_network() &&
         lhs.accessed_network() == rhs.accessed_network();
}

bool operator==(const PreconnectPrediction& lhs,
                const PreconnectPrediction& rhs) {
  return lhs.is_redirected == rhs.is_redirected && lhs.host == rhs.host &&
         lhs.preconnect_origins == rhs.preconnect_origins &&
         lhs.preresolve_hosts == rhs.preresolve_hosts;
}

}  // namespace predictors
