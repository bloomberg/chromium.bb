// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/reporting/reporting_policy.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_config_service_fixed.h"
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

namespace net {
namespace {

const char kGroup[] = "network-errors";
const int kMaxAgeSec = 86400;

const char kConfigurePath[] = "/configure";
const char kFailPath[] = "/fail";
const char kReportPath[] = "/report";

class HungHttpResponse : public test_server::HttpResponse {
 public:
  HungHttpResponse() = default;

  void SendResponse(const test_server::SendBytesCallback& send,
                    const test_server::SendCompleteCallback& done) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(HungHttpResponse);
};

class NetworkErrorLoggingEndToEndTest : public ::testing::Test {
 protected:
  NetworkErrorLoggingEndToEndTest()
      : test_server_(test_server::EmbeddedTestServer::TYPE_HTTPS),
        upload_should_hang_(false),
        upload_received_(false) {
    // Make report delivery happen instantly.
    auto policy = std::make_unique<ReportingPolicy>();
    policy->delivery_interval = base::TimeDelta::FromSeconds(0);

    URLRequestContextBuilder builder;
#if defined(OS_LINUX) || defined(OS_ANDROID)
    builder.set_proxy_config_service(
        std::make_unique<ProxyConfigServiceFixed>(ProxyConfig::CreateDirect()));
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)
    builder.set_reporting_policy(std::move(policy));
    builder.set_network_error_logging_enabled(true);
    url_request_context_ = builder.Build();

    EXPECT_TRUE(url_request_context_->reporting_service());
    EXPECT_TRUE(url_request_context_->network_error_logging_delegate());

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &NetworkErrorLoggingEndToEndTest::HandleConfigureRequest,
        base::Unretained(this)));
    test_server_.RegisterRequestHandler(
        base::BindRepeating(&NetworkErrorLoggingEndToEndTest::HandleFailRequest,
                            base::Unretained(this)));
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &NetworkErrorLoggingEndToEndTest::HandleReportRequest,
        base::Unretained(this)));
    EXPECT_TRUE(test_server_.Start());
  }

  GURL GetConfigureURL() { return test_server_.GetURL(kConfigurePath); }

  GURL GetFailURL() { return test_server_.GetURL(kFailPath); }

  GURL GetReportURL() { return test_server_.GetURL(kReportPath); }

  std::unique_ptr<test_server::HttpResponse> HandleConfigureRequest(
      const test_server::HttpRequest& request) {
    if (request.relative_url != kConfigurePath)
      return nullptr;

    GURL endpoint_url = GetReportURL();

    auto response = std::make_unique<test_server::BasicHttpResponse>();
    response->AddCustomHeader(
        "Report-To",
        base::StringPrintf("{\"url\":\"%s\",\"group\":\"%s\",\"max-age\":%d}",
                           endpoint_url.spec().c_str(), kGroup, kMaxAgeSec));
    response->AddCustomHeader(
        "NEL", base::StringPrintf("{\"report-to\":\"%s\",\"max-age\":%d}",
                                  kGroup, kMaxAgeSec));
    response->set_content_type("text/plain");
    response->set_content("");
    return std::move(response);
  }

  std::unique_ptr<test_server::HttpResponse> HandleFailRequest(
      const test_server::HttpRequest& request) {
    if (request.relative_url != kFailPath)
      return nullptr;

    return std::make_unique<test_server::RawHttpResponse>("", "");
  }

  std::unique_ptr<test_server::HttpResponse> HandleReportRequest(
      const test_server::HttpRequest& request) {
    if (request.relative_url != kReportPath)
      return nullptr;

    EXPECT_FALSE(upload_received_);
    upload_received_ = true;

    EXPECT_TRUE(request.has_content);
    upload_content_ = request.content;

    if (!upload_closure_.is_null())
      std::move(upload_closure_).Run();

    if (upload_should_hang_)
      return std::make_unique<HungHttpResponse>();

    auto response = std::make_unique<test_server::BasicHttpResponse>();
    response->set_content_type("text/plain");
    response->set_content("");
    return std::move(response);
  }

  std::unique_ptr<URLRequestContext> url_request_context_;
  test_server::EmbeddedTestServer test_server_;

  bool upload_should_hang_;
  bool upload_received_;
  std::string upload_content_;
  base::OnceClosure upload_closure_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkErrorLoggingEndToEndTest);
};

TEST_F(NetworkErrorLoggingEndToEndTest, ReportNetworkError) {
  TestDelegate configure_delegate;
  auto configure_request = url_request_context_->CreateRequest(
      GetConfigureURL(), DEFAULT_PRIORITY, &configure_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  configure_request->set_method("GET");
  configure_request->Start();
  base::RunLoop().Run();
  EXPECT_TRUE(configure_request->status().is_success());

  TestDelegate fail_delegate;
  auto fail_request = url_request_context_->CreateRequest(
      GetFailURL(), DEFAULT_PRIORITY, &fail_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  fail_request->set_method("GET");
  fail_request->Start();
  base::RunLoop().Run();
  EXPECT_EQ(URLRequestStatus::FAILED, fail_request->status().status());
  EXPECT_EQ(ERR_EMPTY_RESPONSE, fail_request->status().error());

  if (!upload_received_) {
    base::RunLoop run_loop;
    upload_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  auto reports = base::test::ParseJson(upload_content_);

  base::ListValue* reports_list;
  ASSERT_TRUE(reports->GetAsList(&reports_list));
  ASSERT_EQ(1u, reports_list->GetSize());
  base::DictionaryValue* report_dict;
  ASSERT_TRUE(reports_list->GetDictionary(0u, &report_dict));

  ExpectDictStringValue("network-error", *report_dict, "type");
  ExpectDictStringValue(GetFailURL().spec(), *report_dict, "url");
  base::DictionaryValue* body_dict;
  ASSERT_TRUE(report_dict->GetDictionary("report", &body_dict));

  ExpectDictStringValue("http.response.empty", *body_dict, "type");
  ExpectDictIntegerValue(0, *body_dict, "status-code");
  ExpectDictStringValue(GetFailURL().spec(), *body_dict, "uri");
}

// Make sure an upload that is in progress at shutdown does not crash.
// This verifies that https://crbug.com/792978 is fixed.
TEST_F(NetworkErrorLoggingEndToEndTest, UploadAtShutdown) {
  upload_should_hang_ = true;

  TestDelegate configure_delegate;
  auto configure_request = url_request_context_->CreateRequest(
      GetConfigureURL(), DEFAULT_PRIORITY, &configure_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  configure_request->set_method("GET");
  configure_request->Start();
  base::RunLoop().Run();
  EXPECT_TRUE(configure_request->status().is_success());

  TestDelegate fail_delegate;
  auto fail_request = url_request_context_->CreateRequest(
      GetFailURL(), DEFAULT_PRIORITY, &fail_delegate,
      TRAFFIC_ANNOTATION_FOR_TESTS);
  fail_request->set_method("GET");
  fail_request->Start();
  base::RunLoop().RunUntilIdle();

  // Let Reporting and NEL shut down with the upload still pending to see if
  // they crash.
}

}  // namespace
}  // namespace net
