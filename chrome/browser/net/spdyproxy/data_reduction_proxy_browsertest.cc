// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace data_reduction_proxy {
namespace {

using testing::HasSubstr;
using testing::Not;

constexpr char kSessionKey[] = "TheSessionKeyYay!";
constexpr char kMockHost[] = "mock.host";
constexpr char kDummyBody[] = "dummy";
constexpr char kPrimaryResponse[] = "primary";
constexpr char kSecondaryResponse[] = "secondary";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const std::string& content,
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(content);
  response->set_content_type("text/plain");
  return response;
}

std::unique_ptr<net::test_server::HttpResponse> IncrementRequestCount(
    const std::string& relative_url,
    int* request_count,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == relative_url)
    (*request_count)++;
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

}  // namespace

class DataReductionProxyBrowsertest : public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    net::HostPortPair host_port_pair = embedded_test_server()->host_port_pair();
    std::string config = EncodeConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        host_port_pair.host(), host_port_pair.port(), ProxyServer::CORE,
        ProxyServer_ProxyScheme_HTTP, "fallback.net", 80,
        ProxyServer::UNSPECIFIED_TYPE, 0.5f, false));
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyServerClientConfig, config);
    command_line->AppendSwitch(
        switches::kDisableDataReductionProxyWarmupURLFetch);
    command_line->AppendSwitchASCII(
        network::switches::kForceEffectiveConnectionType, "4G");
  }

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDataReductionProxyEnabledWithNetworkService);
    param_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDataReductionProxyRobustConnection,
        {{params::GetMissingViaBypassParamName(), "true"},
         {params::GetWarmupCallbackParamName(), "true"}});
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule(kMockHost, "127.0.0.1");
    EnableDataSaver(true);
  }

 protected:
  void EnableDataSaver(bool enabled) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(::prefs::kDataSaverEnabled, enabled);
    base::RunLoop().RunUntilIdle();
  }

  std::string GetBody() {
    std::string body;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.textContent);",
        &body));
    return body;
  }

  GURL GetURLWithMockHost(const net::EmbeddedTestServer& server,
                          const std::string& relative_url) {
    GURL server_base_url = server.base_url();
    GURL base_url =
        GURL(base::StrCat({server_base_url.scheme(), "://", kMockHost, ":",
                           server_base_url.port()}));
    EXPECT_TRUE(base_url.is_valid()) << base_url.possibly_invalid_spec();
    return base_url.Resolve(relative_url);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedFeatureList param_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, ChromeProxyHeaderSet) {
  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));

  std::string body = GetBody();
  EXPECT_THAT(body, HasSubstr(kSessionKey));
  EXPECT_THAT(body, HasSubstr("pid="));
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ChromeProxyHeaderSetForSubresource) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));

  std::string script = R"((url => {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.onload = () => domAutomationController.send(xhr.responseText);
    xhr.send();
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script + "('" +
          GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy").spec() +
          "')",
      &result));

  EXPECT_THAT(result, HasSubstr(kSessionKey));
  EXPECT_THAT(result, Not(HasSubstr("pid=")));
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest, ChromeProxyEctHeaderSet) {
  // Proxy will be used, so it shouldn't matter if the host cannot be resolved.
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy-Ect"));

  EXPECT_EQ(GetBody(), "4G");
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyBrowsertest,
                       ProxyNotUsedWhenDisabled) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), testing::HasSubstr(kSessionKey));

  EnableDataSaver(false);

  // |test_server| only has the BasicResponse handler, so should return the
  // dummy response no matter what the URL if it is not being proxied.
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(), kDummyBody);
}

class DataReductionProxyFallbackBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DataReductionProxyBrowsertest::SetUpCommandLine(command_line);

    // Set up a primary server which will return the Chrome-Proxy header set by
    // SetHeader() and status set by SetStatusCode(). Secondary server will just
    // return the secondary response.
    primary_server_.RegisterRequestHandler(base::BindRepeating(
        &DataReductionProxyFallbackBrowsertest::AddChromeProxyHeader,
        base::Unretained(this)));
    ASSERT_TRUE(primary_server_.Start());

    secondary_server_.RegisterRequestHandler(
        base::BindRepeating(&BasicResponse, kSecondaryResponse));
    ASSERT_TRUE(secondary_server_.Start());

    net::HostPortPair primary_host_port_pair = primary_server_.host_port_pair();
    net::HostPortPair secondary_host_port_pair =
        secondary_server_.host_port_pair();
    std::string config = EncodeConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        primary_host_port_pair.host(), primary_host_port_pair.port(),
        ProxyServer::CORE, ProxyServer_ProxyScheme_HTTP,
        secondary_host_port_pair.host(), secondary_host_port_pair.port(),
        ProxyServer::CORE, 0.5f, false));
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyServerClientConfig, config);
  }

  void SetHeader(const std::string& header) { header_ = header; }

  void SetStatusCode(net::HttpStatusCode status_code) {
    status_code_ = status_code;
  }

 private:
  std::unique_ptr<net::test_server::HttpResponse> AddChromeProxyHeader(
      const net::test_server::HttpRequest& request) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (!header_.empty())
      response->AddCustomHeader(chrome_proxy_header(), header_);
    response->set_code(status_code_);
    response->set_content(kPrimaryResponse);
    response->set_content_type("text/plain");
    return response;
  }

  net::HttpStatusCode status_code_ = net::HTTP_OK;
  std::string header_;
  net::EmbeddedTestServer primary_server_;
  net::EmbeddedTestServer secondary_server_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedOn500Status) {
  // Should fall back to the secondary proxy if a 500 error occurs.
  SetStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);

  // Bad proxy should still be bypassed.
  SetStatusCode(net::HTTP_OK);
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBypassHeaderSent) {
  // Should fall back to the secondary proxy if the bypass header is set.
  SetHeader("bypass=100");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);

  // Bad proxy should still be bypassed.
  SetHeader("");
  ui_test_utils::NavigateToURL(
      browser(), GURL("http://does.not.resolve/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), kSecondaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       NoProxyUsedWhenBlockOnceHeaderSent) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy.
  SetHeader("block-once");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);

  // Proxy should no longer be blocked, and use first proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_EQ(GetBody(), kPrimaryResponse);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockHeaderSent) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy.
  SetHeader("block=100");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

IN_PROC_BROWSER_TEST_F(DataReductionProxyFallbackBrowsertest,
                       FallbackProxyUsedWhenBlockZeroHeaderSent) {
  net::EmbeddedTestServer test_server;
  test_server.RegisterRequestHandler(
      base::BindRepeating(&BasicResponse, kDummyBody));
  ASSERT_TRUE(test_server.Start());

  // Request should not use a proxy. Sending 0 for the block param will block
  // requests for a random duration between 1 and 5 minutes.
  SetHeader("block=0");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);

  // Request should still not use proxy.
  SetHeader("");
  ui_test_utils::NavigateToURL(browser(),
                               GetURLWithMockHost(test_server, "/echo"));
  EXPECT_THAT(GetBody(), kDummyBody);
}

class DataReductionProxyResourceTypeBrowsertest
    : public DataReductionProxyBrowsertest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DataReductionProxyBrowsertest::SetUpCommandLine(command_line);

    // Two proxies are set up here, one with type CORE and one UNSPECIFIED_TYPE.
    // The CORE proxy is the secondary, and should be used for requests from the
    // <video> tag.
    unspecified_server_.RegisterRequestHandler(base::BindRepeating(
        &IncrementRequestCount, "/video", &unspecified_request_count_));
    ASSERT_TRUE(unspecified_server_.Start());

    core_server_.RegisterRequestHandler(base::BindRepeating(
        &IncrementRequestCount, "/video", &core_request_count_));
    ASSERT_TRUE(core_server_.Start());

    net::HostPortPair unspecified_host_port_pair =
        unspecified_server_.host_port_pair();
    net::HostPortPair core_host_port_pair = core_server_.host_port_pair();
    std::string config = EncodeConfig(CreateConfig(
        kSessionKey, 1000, 0, ProxyServer_ProxyScheme_HTTP,
        unspecified_host_port_pair.host(), unspecified_host_port_pair.port(),
        ProxyServer::UNSPECIFIED_TYPE, ProxyServer_ProxyScheme_HTTP,
        core_host_port_pair.host(), core_host_port_pair.port(),
        ProxyServer::CORE, 0.5f, false));
    command_line->AppendSwitchASCII(
        switches::kDataReductionProxyServerClientConfig, config);
  }

  int unspecified_request_count_ = 0;
  int core_request_count_ = 0;

 private:
  net::EmbeddedTestServer unspecified_server_;
  net::EmbeddedTestServer core_server_;
};

IN_PROC_BROWSER_TEST_F(DataReductionProxyResourceTypeBrowsertest,
                       CoreProxyUsedForMedia) {
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(*embedded_test_server(), "/echo"));

  std::string script = R"((url => {
    var video = document.createElement('video');
    // Use onerror since the response is not a valid video.
    video.onerror = () => domAutomationController.send('done');
    video.src = url;
    video.load();
    document.body.appendChild(video);
  }))";
  std::string result;
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      script + "('" +
          GetURLWithMockHost(*embedded_test_server(), "/video").spec() + "')",
      &result));
  EXPECT_EQ(result, "done");

  EXPECT_EQ(unspecified_request_count_, 0);
  EXPECT_EQ(core_request_count_, 1);
}

}  // namespace data_reduction_proxy
