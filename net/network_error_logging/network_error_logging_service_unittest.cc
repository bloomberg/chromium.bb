// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_service.h"
#include "net/socket/next_proto.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

class TestReportingService : public ReportingService {
 public:
  struct Report {
    Report() = default;

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

    ~Report() = default;

    GURL url;
    std::string group;
    std::string type;
    std::unique_ptr<const base::Value> body;

   private:
    DISALLOW_COPY(Report);
  };

  TestReportingService() = default;

  const std::vector<Report>& reports() const { return reports_; }

  // ReportingService implementation:

  ~TestReportingService() override = default;

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

  void RemoveBrowsingData(int data_type_mask,
                          const base::RepeatingCallback<bool(const GURL&)>&
                              origin_filter) override {
    NOTREACHED();
  }

  bool RequestIsUpload(const URLRequest& request) override {
    NOTREACHED();
    return true;
  }

  const ReportingPolicy& GetPolicy() const override {
    NOTREACHED();
    return dummy_policy_;
  }

 private:
  std::vector<Report> reports_;
  ReportingPolicy dummy_policy_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingService);
};

class NetworkErrorLoggingServiceTest : public ::testing::Test {
 protected:
  NetworkErrorLoggingServiceTest() {
    service_ = NetworkErrorLoggingService::Create();
    CreateReportingService();
  }

  void CreateReportingService() {
    DCHECK(!reporting_service_);

    reporting_service_ = std::make_unique<TestReportingService>();
    service_->SetReportingService(reporting_service_.get());
  }

  void DestroyReportingService() {
    DCHECK(reporting_service_);

    service_->SetReportingService(nullptr);
    reporting_service_.reset();
  }

  NetworkErrorLoggingDelegate::ErrorDetails MakeErrorDetails(GURL url,
                                                             Error error_type) {
    NetworkErrorLoggingDelegate::ErrorDetails details;

    details.uri = url;
    details.referrer = kReferrer_;
    details.server_ip = IPAddress::IPv4AllZeros();
    details.protocol = kProtoUnknown;
    details.status_code = 0;
    details.elapsed_time = base::TimeDelta::FromSeconds(1);
    details.type = error_type;
    details.is_reporting_upload = false;

    return details;
  }

  NetworkErrorLoggingService* service() { return service_.get(); }
  const std::vector<TestReportingService::Report>& reports() {
    return reporting_service_->reports();
  }

  const GURL kUrl_ = GURL("https://example.com/path");
  const GURL kUrlDifferentPort_ = GURL("https://example.com:4433/path");
  const GURL kUrlSubdomain_ = GURL("https://subdomain.example.com/path");
  const GURL kUrlDifferentHost_ = GURL("https://example2.com/path");

  const url::Origin kOrigin_ = url::Origin::Create(kUrl_);
  const url::Origin kOriginDifferentPort_ =
      url::Origin::Create(kUrlDifferentPort_);
  const url::Origin kOriginSubdomain_ = url::Origin::Create(kUrlSubdomain_);
  const url::Origin kOriginDifferentHost_ =
      url::Origin::Create(kUrlDifferentHost_);

  const std::string kHeader_ = "{\"report-to\":\"group\",\"max-age\":86400}";
  const std::string kHeaderIncludeSubdomains_ =
      "{\"report-to\":\"group\",\"max-age\":86400,\"includeSubdomains\":true}";
  const std::string kHeaderFailureFraction0_ =
      "{\"report-to\":\"group\",\"max-age\":86400,\"failure-fraction\":0.0}";
  const std::string kHeaderFailureFractionHalf_ =
      "{\"report-to\":\"group\",\"max-age\":86400,\"failure-fraction\":0.5}";
  const std::string kHeaderMaxAge0_ = "{\"max-age\":0}";

  const std::string kGroup_ = "group";

  const std::string kType_ = NetworkErrorLoggingService::kReportType;

  const GURL kReferrer_ = GURL("https://referrer.com/");

 private:
  std::unique_ptr<NetworkErrorLoggingService> service_;
  std::unique_ptr<TestReportingService> reporting_service_;
};

void ExpectDictDoubleValue(double expected_value,
                           const base::DictionaryValue& value,
                           const std::string& key) {
  double double_value = 0.0;
  EXPECT_TRUE(value.GetDouble(key, &double_value)) << key;
  EXPECT_DOUBLE_EQ(expected_value, double_value) << key;
}

TEST_F(NetworkErrorLoggingServiceTest, CreateService) {
  // Service is created by default in the test fixture..
  EXPECT_TRUE(service());
}

TEST_F(NetworkErrorLoggingServiceTest, NoReportingService) {
  DestroyReportingService();

  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));
}

TEST_F(NetworkErrorLoggingServiceTest, OriginInsecure) {
  const GURL kInsecureUrl("http://insecure.com/");
  const url::Origin kInsecureOrigin = url::Origin::Create(kInsecureUrl);

  service()->OnHeader(kInsecureOrigin, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kInsecureUrl, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, NoPolicyForOrigin) {
  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, ReportQueued) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrl_, reports()[0].url);
  EXPECT_EQ(kGroup_, reports()[0].group);
  EXPECT_EQ(kType_, reports()[0].type);

  const base::DictionaryValue* body;
  ASSERT_TRUE(reports()[0].body->GetAsDictionary(&body));
  base::ExpectDictStringValue(kUrl_.spec(), *body,
                              NetworkErrorLoggingService::kUriKey);
  base::ExpectDictStringValue(kReferrer_.spec(), *body,
                              NetworkErrorLoggingService::kReferrerKey);
  // TODO(juliatuttle): Extract these constants.
  ExpectDictDoubleValue(1.0, *body,
                        NetworkErrorLoggingService::kSamplingFractionKey);
  base::ExpectDictStringValue("0.0.0.0", *body,
                              NetworkErrorLoggingService::kServerIpKey);
  base::ExpectDictStringValue("", *body,
                              NetworkErrorLoggingService::kProtocolKey);
  base::ExpectDictIntegerValue(0, *body,
                               NetworkErrorLoggingService::kStatusCodeKey);
  base::ExpectDictIntegerValue(1000, *body,
                               NetworkErrorLoggingService::kElapsedTimeKey);
  base::ExpectDictStringValue("tcp.refused", *body,
                              NetworkErrorLoggingService::kTypeKey);
}

TEST_F(NetworkErrorLoggingServiceTest, MaxAge0) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnHeader(kOrigin_, kHeaderMaxAge0_);

  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, FailureFraction0) {
  service()->OnHeader(kOrigin_, kHeaderFailureFraction0_);

  // Each network error has a 0% chance of being reported.  Fire off several and
  // verify that no reports are produced.
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, FailureFractionHalf) {
  service()->OnHeader(kOrigin_, kHeaderFailureFractionHalf_);

  // Each network error has a 50% chance of being reported.  Fire off several
  // and verify that some requests were reported and some weren't.  (We can't
  // verify exact counts because each decision is made randomly.)
  constexpr size_t kReportCount = 100;
  for (size_t i = 0; i < kReportCount; ++i)
    service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  // If our random selection logic is correct, there is a 2^-100 chance that
  // every single report above was skipped.  If this check fails, it's much more
  // likely that our code is wrong.
  EXPECT_FALSE(reports().empty());

  // There's also a 2^-100 chance that every single report was logged.  Same as
  // above, that's much more likely to be a code error.
  EXPECT_GT(kReportCount, reports().size());

  for (const auto& report : reports()) {
    const base::DictionaryValue* body;
    ASSERT_TRUE(report.body->GetAsDictionary(&body));
    ExpectDictDoubleValue(0.5, *body,
                          NetworkErrorLoggingService::kSamplingFractionKey);
  }
}

TEST_F(NetworkErrorLoggingServiceTest,
       ExcludeSubdomainsDoesntMatchDifferentPort) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlDifferentPort_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, ExcludeSubdomainsDoesntMatchSubdomain) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlSubdomain_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesDifferentPort) {
  service()->OnHeader(kOrigin_, kHeaderIncludeSubdomains_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlDifferentPort_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
  EXPECT_EQ(kUrlDifferentPort_, reports()[0].url);
}

TEST_F(NetworkErrorLoggingServiceTest, IncludeSubdomainsMatchesSubdomain) {
  service()->OnHeader(kOrigin_, kHeaderIncludeSubdomains_);

  service()->OnNetworkError(
      MakeErrorDetails(kUrlSubdomain_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
}

TEST_F(NetworkErrorLoggingServiceTest,
       IncludeSubdomainsDoesntMatchSuperdomain) {
  service()->OnHeader(kOriginSubdomain_, kHeaderIncludeSubdomains_);

  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, RemoveAllBrowsingData) {
  service()->OnHeader(kOrigin_, kHeader_);

  service()->RemoveBrowsingData(base::RepeatingCallback<bool(const GURL&)>());

  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());
}

TEST_F(NetworkErrorLoggingServiceTest, RemoveSomeBrowsingData) {
  service()->OnHeader(kOrigin_, kHeader_);
  service()->OnHeader(kOriginDifferentHost_, kHeader_);

  service()->RemoveBrowsingData(
      base::BindRepeating([](const GURL& origin) -> bool {
        return origin.host() == "example.com";
      }));

  service()->OnNetworkError(MakeErrorDetails(kUrl_, ERR_CONNECTION_REFUSED));

  EXPECT_TRUE(reports().empty());

  service()->OnNetworkError(
      MakeErrorDetails(kUrlDifferentHost_, ERR_CONNECTION_REFUSED));

  EXPECT_EQ(1u, reports().size());
}

}  // namespace
}  // namespace net
