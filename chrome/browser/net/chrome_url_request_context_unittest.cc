// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_url_request_context.h"

#include "base/command_line.h"
#include "base/format_macros.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/testing_pref_service.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) StringPrintf("at line %d <%s>", __LINE__, desc)

TEST(ChromeURLRequestContextTest, CreateProxyConfigTest) {
  FilePath unused_path(FILE_PATH_LITERAL("foo.exe"));
  // Build the input command lines here.
  CommandLine empty(unused_path);
  CommandLine no_proxy(unused_path);
  no_proxy.AppendSwitch(switches::kNoProxyServer);
  CommandLine no_proxy_extra_params(unused_path);
  no_proxy_extra_params.AppendSwitch(switches::kNoProxyServer);
  no_proxy_extra_params.AppendSwitchASCII(switches::kProxyServer,
                                          "http://proxy:8888");
  CommandLine single_proxy(unused_path);
  single_proxy.AppendSwitchASCII(switches::kProxyServer, "http://proxy:8888");
  CommandLine per_scheme_proxy(unused_path);
  per_scheme_proxy.AppendSwitchASCII(switches::kProxyServer,
                                     "http=httpproxy:8888;ftp=ftpproxy:8889");
  CommandLine per_scheme_proxy_bypass(unused_path);
  per_scheme_proxy_bypass.AppendSwitchASCII(
      switches::kProxyServer,
      "http=httpproxy:8888;ftp=ftpproxy:8889");
  per_scheme_proxy_bypass.AppendSwitchASCII(
      switches::kProxyBypassList,
      ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8");
  CommandLine with_pac_url(unused_path);
  with_pac_url.AppendSwitchASCII(switches::kProxyPacUrl, "http://wpad/wpad.dat");
  with_pac_url.AppendSwitchASCII(
      switches::kProxyBypassList,
      ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8");
  CommandLine with_auto_detect(unused_path);
  with_auto_detect.AppendSwitch(switches::kProxyAutoDetect);

  // Inspired from proxy_config_service_win_unittest.cc.
  const struct {
    // Short description to identify the test
    std::string description;

    // The command line to build a ProxyConfig from.
    const CommandLine& command_line;

    // Expected outputs (fields of the ProxyConfig).
    bool is_null;
    bool auto_detect;
    GURL pac_url;
    net::ProxyRulesExpectation proxy_rules;
  } tests[] = {
    {
      TEST_DESC("Empty command line"),
      // Input
      empty,
      // Expected result
      true,                                               // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::Empty(),
    },
    {
      TEST_DESC("No proxy"),
      // Input
      no_proxy,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::Empty(),
    },
    {
      TEST_DESC("No proxy with extra parameters."),
      // Input
      no_proxy_extra_params,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::Empty(),
    },
    {
      TEST_DESC("Single proxy."),
      // Input
      single_proxy,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::Single(
          "proxy:8888",  // single proxy
          ""),           // bypass rules
    },
    {
      TEST_DESC("Per scheme proxy."),
      // Input
      per_scheme_proxy,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::PerScheme(
          "httpproxy:8888",  // http
          "",                // https
          "ftpproxy:8889",   // ftp
          ""),               // bypass rules
    },
    {
      TEST_DESC("Per scheme proxy with bypass URLs."),
      // Input
      per_scheme_proxy_bypass,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::PerScheme(
          "httpproxy:8888",  // http
          "",                // https
          "ftpproxy:8889",   // ftp
          "*.google.com,foo.com:99,1.2.3.4:22,127.0.0.1/8"),
    },
    {
      TEST_DESC("Pac URL with proxy bypass URLs"),
      // Input
      with_pac_url,
      // Expected result
      false,                                              // is_null
      false,                                              // auto_detect
      GURL("http://wpad/wpad.dat"),                       // pac_url
      net::ProxyRulesExpectation::EmptyWithBypass(
          "*.google.com,foo.com:99,1.2.3.4:22,127.0.0.1/8"),
    },
    {
      TEST_DESC("Autodetect"),
      // Input
      with_auto_detect,
      // Expected result
      false,                                              // is_null
      true,                                               // auto_detect
      GURL(),                                             // pac_url
      net::ProxyRulesExpectation::Empty(),
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); i++) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));
    CommandLine command_line(tests[i].command_line);
    // Only configuration-policy and default prefs are needed.
    PrefService prefs(new TestingPrefService::TestingPrefValueStore(
        new policy::ConfigurationPolicyPrefStore(NULL),
        new policy::ConfigurationPolicyPrefStore(NULL), NULL,
        new CommandLinePrefStore(&command_line), NULL, NULL,
        new DefaultPrefStore()));
    ChromeURLRequestContextGetter::RegisterUserPrefs(&prefs);
    scoped_ptr<net::ProxyConfig> config(CreateProxyConfig(&prefs));

    if (tests[i].is_null) {
      EXPECT_TRUE(config == NULL);
    } else {
      EXPECT_TRUE(config != NULL);
      EXPECT_EQ(tests[i].auto_detect, config->auto_detect());
      EXPECT_EQ(tests[i].pac_url, config->pac_url());
      EXPECT_TRUE(tests[i].proxy_rules.Matches(config->proxy_rules()));
    }
  }
}
