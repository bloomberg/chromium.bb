// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/network_error_logging/network_error_logging_service.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/reporting/reporting_service.h"
#include "net/socket/next_proto.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class TestReportingService : public net::ReportingService {
 public:
  struct Report {
    Report() {}

    Report(Report&& other)
        : url(other.url),
          group(other.group),
          type(other.type),
          body(std::move(other.body)) {}

    Report(const GURL& url,
           const std::string& group,
           const std::string& type,
           std::unique_ptr<const base::Value> body)
        : url(url), group(group), type(type), body(std::move(body)) {}

    ~Report() {}

    GURL url;
    std::string group;
    std::string type;
    std::unique_ptr<const base::Value> body;

   private:
    DISALLOW_COPY(Report);
  };

  TestReportingService() {}

  const std::vector<Report>& reports() const { return reports_; }

  // net::ReportingService implementation:

  ~TestReportingService() override {}

  void QueueReport(const GURL& url,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body) override {
    reports_.push_back(Report(url, group, type, std::move(body)));
  }

  void ProcessHeader(const GURL& url,
                     const std::string& header_value) override {
    NOTREACHED();
  }

  void RemoveBrowsingData(
      int data_type_mask,
      base::Callback<bool(const GURL&)> origin_filter) override {
    NOTREACHED();
  }

 private:
  std::vector<Report> reports_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingService);
};

class NetworkErrorLoggingServiceTest : public ::testing::Test {
 protected:
  NetworkErrorLoggingServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNetworkErrorLogging);
    service_ = network_error_logging::NetworkErrorLoggingService::Create();
    CreateReportingService();
  }

  void CreateReportingService() {
    DCHECK(!reporting_service_);

    reporting_service_ = base::MakeUnique<TestReportingService>();
    service_->SetReportingService(reporting_service_.get());
  }

  void DestroyReportingService() {
    DCHECK(reporting_service_);

    service_->SetReportingService(nullptr);
    reporting_service_.reset();
  }

  net::NetworkErrorLoggingDelegate::ErrorDetails MakeErrorDetails(
      GURL url,
      net::Error error_type) {
    net::NetworkErrorLoggingDelegate::ErrorDetails details;

    details.uri = url;
    details.referrer = kReferrer_;
    details.server_ip = net::IPAddress::IPv4AllZeros();
    details.protocol = net::kProtoUnknown;
    details.status_code = 0;
    details.elapsed_time = base::TimeDelta::FromSeconds(1);
    details.type = error_type;

    return details;
  }

  network_error_logging::NetworkErrorLoggingService* service() {
    return service_.get();
  }
  const std::vector<TestReportingService::Report>& reports() {
    return reporting_service_->reports();
  }

  const GURL kUrl_ = GURL("https://example.com/path");
  const GURL kUrlDifferentPort_ = GURL("https://example.com:4433/path");
  const GURL kUrlSubdomain_ = GURL("https://subdomain.example.com/path");

  const url::Origin kOrigin_ = url::Origin::Create(kUrl_);
  const url::Origin kOriginDifferentPort_ =
      url::Origin::Create(kUrlDifferentPort_);
  const url::Origin kOriginSubdomain_ = url::Origin::Create(kUrlSubdomain_);

  const std::string kHeader_ = "{\"report-to\":\"group\",\"max-age\":86400}";
  const std::string kHeaderIncludeSubdomains_ =
      "{\"report-to\":\"group\",\"max-age\":86400,\"includeSubdomains\":true}";
  const std::string kHeaderMaxAge0_ = "{\"max-age\":0}";

  const std::string kGroup_ = "group";

  const std::string kType_ =
      network_error_logging::NetworkErrorLoggingService::kReportType;

  const GURL kReferrer_ = GURL("https://referrer.com/");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<network_error_logging::NetworkErrorLoggingService> service_;
  std::unique_ptr<TestReportingService> reporting_service_;
};

TEST_F(NetworkErrorLoggingServiceTest, FeatureDisabled) {
  // N.B. This test does not actually use the test fixture.

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kNetworkErrorLogging);

  auto service = network_error_logging::NetworkErrorLoggingService::Create();
  EXPECT_FALSE(service);
}

TEST_F(NetworkErrorLoggingServiceTest, FeatureEnabled) {
  EXPECT_TRUE(service());
}

TEST_F(NetworkErrorLoggingServiceTest, NoReportingService) {
  DestroyReportingService();

  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrl_, net::ERR_CONNECTION_REFUSED));
}

TEST_F(NetworkErrorLoggingServiceTest, OriginInsecure) {
  const GURL kInsecureUrl("http://insecure.com/");
  const url::Origin kInsecureOrigin = url::Origin::Create(kInsecureUrl);

  service()->OnHeader(kInsecureOrigin, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kInsecureUrl, net::ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, NoPolicyForOrigin) {
  service()->OnNetworkError(
      MakeErrorDetails(kUrl_, net::ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, ReportQueued) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrl_, net::ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(
      kUrl_.spec(), *body,
      network_error_logging::NetworkErrorLoggingService::kUriKey);
  base::ExpectDictStringValue(
      kReferrer_.spec(), *body,
      network_error_logging::NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  base::ExpectDictStringValue(
      "0.0.0.0", *body,
      network_error_logging::NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue(
      "", *body,
      network_error_logging::NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictIntegerValue(
      0, *body,
      network_error_logging::NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(
      1000, *body,
      network_error_logging::NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue(
      "tcp.refused", *body,
      network_error_logging::NetworkErrorLoggingService::kTypeKey);
}

TEST_F(NetworkErrorLoggingServiceTest, MaxAge0) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnHeader(kOrigin_, kHeaderMaxAge0_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrl_, net::ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest,
       ExcludeSubdomainsDoesntMatchDifferentPort) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlDifferentPort_, net::ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, ExcludeSubdomainsDoesntMatchSubdomain) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlSubdomain_, net::ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesDifferentPort) {
  service()->OnHeader(kOrigin_, kHeaderIncludeSubdomains_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlDifferentPort_, net::ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrlDifferentPort_, reports()[0].url);
}

TEST_F(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesSubdomain) {
  service()->OnHeader(kOrigin_, kHeaderIncludeSubdomains_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlSubdomain_, net::ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
}

TEST_F(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntMatchSuperdomain) {
  service()->OnHeader(kOriginSubdomain_, kHeaderIncludeSubdomains_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrl_, net::ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

}  // namespace
