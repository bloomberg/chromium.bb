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

std::unique_ptr<net::test_server::HttpResponse> EchoDummy(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content(kDummyBody);
  response->set_content_type("text/plain");
  return response;
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
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kDataReductionProxyRobustConnection,
        {{params::GetMissingViaBypassParamName(), "true"},
         {params::GetWarmupCallbackParamName(), "true"}});
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
  test_server.RegisterRequestHandler(base::BindRepeating(&EchoDummy));
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
  test_server.RegisterRequestHandler(base::BindRepeating(&EchoDummy));
  ASSERT_TRUE(test_server.Start());

  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_THAT(GetBody(), testing::HasSubstr(kSessionKey));

  EnableDataSaver(false);

  // |test_server| only has the EchoDummy handler, so should return the dummy
  // response no matter what the URL if it is not being proxied.
  ui_test_utils::NavigateToURL(
      browser(), GetURLWithMockHost(test_server, "/echoheader?Chrome-Proxy"));
  EXPECT_EQ(GetBody(), kDummyBody);
}

}  // namespace data_reduction_proxy
