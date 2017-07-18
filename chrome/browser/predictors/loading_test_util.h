// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PREDICTORS_LOADING_TEST_UTIL_H_
#define CHROME_BROWSER_PREDICTORS_LOADING_TEST_UTIL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "components/sessions/core/session_id.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace predictors {

// Does nothing, controls which URLs are prefetchable.
class MockResourcePrefetchPredictor : public ResourcePrefetchPredictor {
 public:
  MockResourcePrefetchPredictor(const LoadingPredictorConfig& config,
                                Profile* profile);
  ~MockResourcePrefetchPredictor();

  virtual void RecordPageRequestSummary(
      std::unique_ptr<PageRequestSummary> summary) {
    RecordPageRequestSummaryProxy(summary.get());
  }

  MOCK_CONST_METHOD2(GetPrefetchData,
                     bool(const GURL&, ResourcePrefetchPredictor::Prediction*));
  MOCK_METHOD1(RecordPageRequestSummaryProxy, void(PageRequestSummary*));
};

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

void InitializeOriginStat(OriginStat* origin_stat,
                          const std::string& origin,
                          int number_of_hits,
                          int number_of_misses,
                          int consecutive_misses,
                          double average_position,
                          bool always_access_network,
                          bool accessed_network);

PrefetchData CreatePrefetchData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);
RedirectData CreateRedirectData(const std::string& primary_key,
                                uint64_t last_visit_time = 0);
OriginData CreateOriginData(const std::string& host,
                            uint64_t last_visit_time = 0);

NavigationID CreateNavigationID(SessionID::id_type tab_id,
                                const std::string& main_frame_url);

PageRequestSummary CreatePageRequestSummary(
    const std::string& main_frame_url,
    const std::string& initial_url,
    const std::vector<URLRequestSummary>& subresource_requests);

URLRequestSummary CreateURLRequestSummary(
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

URLRequestSummary CreateRedirectRequestSummary(
    SessionID::id_type session_id,
    const std::string& main_frame_url,
    const std::string& redirect_url);

ResourcePrefetchPredictor::Prediction CreatePrediction(
    const std::string& main_frame_key,
    std::vector<GURL> subresource_urls);

PreconnectPrediction CreatePreconnectPrediction(
    std::string host,
    bool is_redirected,
    std::vector<GURL> preconnect_urls,
    std::vector<GURL> preresolve_urls);

void PopulateTestConfig(LoadingPredictorConfig* config, bool small_db = true);

scoped_refptr<net::HttpResponseHeaders> MakeResponseHeaders(
    const char* headers);

class MockURLRequestJob : public net::URLRequestJob {
 public:
  MockURLRequestJob(net::URLRequest* request,
                    const net::HttpResponseInfo& response_info,
                    const std::string& mime_type);

  bool GetMimeType(std::string* mime_type) const override;

 protected:
  void Start() override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;

 private:
  net::HttpResponseInfo response_info_;
  std::string mime_type_;
};

class MockURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  MockURLRequestJobFactory();
  ~MockURLRequestJobFactory() override;

  void Reset();

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override;

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  bool IsHandledProtocol(const std::string& scheme) const override;

  bool IsSafeRedirectTarget(const GURL& location) const override;

  void set_response_info(const net::HttpResponseInfo& response_info) {
    response_info_ = response_info;
  }

  void set_mime_type(const std::string& mime_type) { mime_type_ = mime_type; }

 private:
  net::HttpResponseInfo response_info_;
  std::string mime_type_;
};

std::unique_ptr<net::URLRequest> CreateURLRequest(
    const net::TestURLRequestContext& url_request_context,
    const GURL& url,
    net::RequestPriority priority,
    content::ResourceType resource_type,
    bool is_main_frame);

// For printing failures nicely.
std::ostream& operator<<(std::ostream& stream, const PrefetchData& data);
std::ostream& operator<<(std::ostream& stream, const ResourceData& resource);
std::ostream& operator<<(std::ostream& stream, const RedirectData& data);
std::ostream& operator<<(std::ostream& stream, const RedirectStat& redirect);
std::ostream& operator<<(std::ostream& stream,
                         const PageRequestSummary& summary);
std::ostream& operator<<(std::ostream& stream,
                         const URLRequestSummary& summary);
std::ostream& operator<<(std::ostream& stream, const NavigationID& id);

std::ostream& operator<<(std::ostream& os, const OriginData& data);
std::ostream& operator<<(std::ostream& os, const OriginStat& redirect);
std::ostream& operator<<(std::ostream& os,
                         const PreconnectPrediction& prediction);

bool operator==(const PrefetchData& lhs, const PrefetchData& rhs);
bool operator==(const ResourceData& lhs, const ResourceData& rhs);
bool operator==(const RedirectData& lhs, const RedirectData& rhs);
bool operator==(const RedirectStat& lhs, const RedirectStat& rhs);
bool operator==(const PageRequestSummary& lhs, const PageRequestSummary& rhs);
bool operator==(const URLRequestSummary& lhs, const URLRequestSummary& rhs);
bool operator==(const OriginRequestSummary& lhs,
                const OriginRequestSummary& rhs);
bool operator==(const OriginData& lhs, const OriginData& rhs);
bool operator==(const OriginStat& lhs, const OriginStat& rhs);
bool operator==(const PreconnectPrediction& lhs,
                const PreconnectPrediction& rhs);

}  // namespace predictors

#endif  // CHROME_BROWSER_PREDICTORS_LOADING_TEST_UTIL_H_
