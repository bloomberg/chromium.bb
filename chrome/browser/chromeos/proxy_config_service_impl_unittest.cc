// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/proxy_config_service_impl.h"

#include <map>
#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "net/proxy/proxy_config_service_common_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(NDEBUG)
#include "chrome/common/json_value_serializer.h"
#endif  // !defined(NDEBUG)

namespace chromeos {

namespace {

struct Input {  // Fields of chromeos::ProxyConfigServiceImpl::ProxyConfig.
  ProxyConfigServiceImpl::ProxyConfig::Mode mode;
  const char* pac_url;
  const char* single_uri;
  const char* http_uri;
  const char* https_uri;
  const char* ftp_uri;
  const char* socks_uri;
  const char* bypass_rules;
};

// Builds an identifier for each test in an array.
#define TEST_DESC(desc) base::StringPrintf("at line %d <%s>", __LINE__, desc)

// Shortcuts to declare enums within chromeos's ProxyConfig.
#define MK_MODE(mode) ProxyConfigServiceImpl::ProxyConfig::MODE_##mode
#define MK_SRC(src) ProxyConfigServiceImpl::ProxyConfig::SOURCE_##src
#define MK_SCHM(scheme) net::ProxyServer::SCHEME_##scheme

// Inspired from net/proxy/proxy_config_service_linux_unittest.cc.
const struct {
  // Short description to identify the test
  std::string description;

  bool is_valid;
  bool test_read_write_access;

  Input input;

  // Expected outputs from fields of net::ProxyConfig (via IO).
  bool auto_detect;
  GURL pac_url;
  net::ProxyRulesExpectation proxy_rules;
} tests[] = {
  {
    TEST_DESC("No proxying"),

    true,  // is_valid
    true,  // test_read_write_access

    { // Input.
      MK_MODE(DIRECT),  // mode
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {
    TEST_DESC("Auto detect"),

    true,  // is_valid
    true,  // test_read_write_access

    { // Input.
      MK_MODE(AUTO_DETECT),  // mode
    },

    // Expected result.
    true,                                    // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {
    TEST_DESC("Valid PAC URL"),

    true,  // is_valid
    true,  // test_read_write_access

    { // Input.
      MK_MODE(PAC_SCRIPT),     // mode
      "http://wpad/wpad.dat",  // pac_url
    },

    // Expected result.
    false,                                   // auto_detect
    GURL("http://wpad/wpad.dat"),            // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {
    TEST_DESC("Invalid PAC URL"),

    false,  // is_valid
    false,  // test_read_write_access

    { // Input.
      MK_MODE(PAC_SCRIPT),  // mode
      "wpad.dat",           // pac_url
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Empty(),     // proxy_rules
  },

  {
    TEST_DESC("Single-host in proxy list"),

    true,  // is_valid
    true,  // test_read_write_access

    { // Input.
      MK_MODE(SINGLE_PROXY),  // mode
      NULL,                   // pac_url
      "www.google.com",       // single_uri
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Single(      // proxy_rules
        "www.google.com:80",  // single proxy
        ""),                  // bypass rules
  },

  {
    TEST_DESC("Single-host, different port"),

    true,   // is_valid
    false,  // test_read_write_access

    { // Input.
      MK_MODE(SINGLE_PROXY),  // mode
      NULL,                   // pac_url
      "www.google.com:99",    // single_uri
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Single(      // proxy_rules
        "www.google.com:99",  // single
        ""),                  // bypass rules
  },

  {
    TEST_DESC("Tolerate a scheme"),

    true,   // is_valid
    false,  // test_read_write_access

    { // Input.
      MK_MODE(SINGLE_PROXY),       // mode
      NULL,                        // pac_url
      "http://www.google.com:99",  // single_uri
    },

    // Expected result.
    false,                                   // auto_detect
    GURL(),                                  // pac_url
    net::ProxyRulesExpectation::Single(      // proxy_rules
        "www.google.com:99",  // single proxy
        ""),                  // bypass rules
  },

  {
    TEST_DESC("Per-scheme proxy rules"),

    true,  // is_valid
    true,  // test_read_write_access

    { // Input.
      MK_MODE(PROXY_PER_SCHEME),  // mode
      NULL,                       // pac_url
      NULL,                       // single_uri
      "www.google.com:80",        // http_uri
      "www.foo.com:110",          // https_uri
      "ftp.foo.com:121",          // ftp_uri
      "socks.com:888",            // socks_uri
    },

    // Expected result.
    false,                          // auto_detect
    GURL(),                         // pac_url
    net::ProxyRulesExpectation::PerSchemeWithSocks(  // proxy_rules
        "www.google.com:80",        // http
        "https://www.foo.com:110",  // https
        "ftp.foo.com:121",          // ftp
        "socks5://socks.com:888",   // fallback proxy
        ""),                        // bypass rules
  },

  {
    TEST_DESC("Bypass rules"),

    true,  // is_valid
    true,  // test_read_write_access

    { // Input.
      MK_MODE(SINGLE_PROXY),      // mode
      NULL,                       // pac_url
      "www.google.com",           // single_uri
      NULL, NULL, NULL, NULL,     // per-proto
      ".google.com, foo.com:99, 1.2.3.4:22, 127.0.0.1/8",  // bypass_rules
    },

    // Expected result.
    false,                          // auto_detect
    GURL(),                         // pac_url
    net::ProxyRulesExpectation::Single(                      // proxy_rules
        "www.google.com:80",                                 // single proxy
        "*.google.com,*foo.com:99,1.2.3.4:22,127.0.0.1/8"),  // bypass_rules
  },
};  // tests

}  // namespace

class ProxyConfigServiceImplTest : public PlatformTest {
 protected:
  ProxyConfigServiceImplTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual ~ProxyConfigServiceImplTest() {
    config_service_ = NULL;
    MessageLoop::current()->RunAllPending();
  }

  void CreateConfigService(
      const ProxyConfigServiceImpl::ProxyConfig& init_config) {
    // Instantiate proxy config service with |init_config|.
    config_service_ = new ProxyConfigServiceImpl(init_config);
  }

  void SetAutomaticProxy(
      ProxyConfigServiceImpl::ProxyConfig::Mode mode,
      ProxyConfigServiceImpl::ProxyConfig::Source source,
      const char* pac_url,
      ProxyConfigServiceImpl::ProxyConfig* config,
      ProxyConfigServiceImpl::ProxyConfig::AutomaticProxy* automatic_proxy) {
    config->mode = mode;
    automatic_proxy->source = source;
    if (pac_url)
      automatic_proxy->pac_url = GURL(pac_url);
  }

  void SetManualProxy(
      ProxyConfigServiceImpl::ProxyConfig::Mode mode,
      ProxyConfigServiceImpl::ProxyConfig::Source source,
      const char* server_uri,
      net::ProxyServer::Scheme scheme,
      ProxyConfigServiceImpl::ProxyConfig* config,
      ProxyConfigServiceImpl::ProxyConfig::ManualProxy* manual_proxy) {
    if (!server_uri)
      return;
    config->mode = mode;
    manual_proxy->source = source;
    manual_proxy->server = net::ProxyServer::FromURI(server_uri, scheme);
  }

  void InitConfigWithTestInput(
      const Input& input, ProxyConfigServiceImpl::ProxyConfig::Source source,
      ProxyConfigServiceImpl::ProxyConfig* init_config) {
    switch (input.mode) {
      case MK_MODE(DIRECT):
      case MK_MODE(AUTO_DETECT):
      case MK_MODE(PAC_SCRIPT):
        SetAutomaticProxy(input.mode, source, input.pac_url, init_config,
                          &init_config->automatic_proxy);
        return;
      case MK_MODE(SINGLE_PROXY):
        SetManualProxy(input.mode, source, input.single_uri, MK_SCHM(HTTP),
                       init_config, &init_config->single_proxy);
        break;
      case MK_MODE(PROXY_PER_SCHEME):
        SetManualProxy(input.mode, source, input.http_uri, MK_SCHM(HTTP),
                       init_config, &init_config->http_proxy);
        SetManualProxy(input.mode, source, input.https_uri, MK_SCHM(HTTPS),
                       init_config, &init_config->https_proxy);
        SetManualProxy(input.mode, source, input.ftp_uri, MK_SCHM(HTTP),
                       init_config, &init_config->ftp_proxy);
        SetManualProxy(input.mode, source, input.socks_uri, MK_SCHM(SOCKS5),
                       init_config, &init_config->socks_proxy);
        break;
    }
    if (input.bypass_rules) {
      init_config->bypass_rules.ParseFromStringUsingSuffixMatching(
          input.bypass_rules);
    }
  }

  void TestReadWriteAccessForMode(const Input& input,
      ProxyConfigServiceImpl::ProxyConfig::Source source) {
    // Init config from |source|.
    ProxyConfigServiceImpl::ProxyConfig init_config;
    InitConfigWithTestInput(input, source, &init_config);
    CreateConfigService(init_config);

    ProxyConfigServiceImpl::ProxyConfig config;
    config_service()->UIGetProxyConfig(&config);

    // For owner, write access to config should be equal CanBeWrittenByOwner().
    // For non-owner, config is never writeable.
    bool expected_writeable_by_owner = CanBeWrittenByOwner(source);
    if (config.mode == MK_MODE(PROXY_PER_SCHEME)) {
      if (input.http_uri) {
        EXPECT_EQ(expected_writeable_by_owner,
                  config.CanBeWrittenByUser(true, "http"));
        EXPECT_FALSE(config.CanBeWrittenByUser(false, "http"));
      }
      if (input.https_uri) {
        EXPECT_EQ(expected_writeable_by_owner,
                  config.CanBeWrittenByUser(true, "http"));
        EXPECT_FALSE(config.CanBeWrittenByUser(false, "https"));
      }
      if (input.ftp_uri) {
        EXPECT_EQ(expected_writeable_by_owner,
                  config.CanBeWrittenByUser(true, "http"));
        EXPECT_FALSE(config.CanBeWrittenByUser(false, "ftp"));
      }
      if (input.socks_uri) {
        EXPECT_EQ(expected_writeable_by_owner,
                  config.CanBeWrittenByUser(true, "http"));
        EXPECT_FALSE(config.CanBeWrittenByUser(false, "socks"));
      }
    } else {
      EXPECT_EQ(expected_writeable_by_owner,
                config.CanBeWrittenByUser(true, std::string()));
      EXPECT_FALSE(config.CanBeWrittenByUser(false, std::string()));
    }
  }

  void TestReadWriteAccessForScheme(
      ProxyConfigServiceImpl::ProxyConfig::Source source,
      const char* server_uri,
      const std::string& scheme) {
    // Init with manual |scheme| proxy.
    ProxyConfigServiceImpl::ProxyConfig init_config;
    ProxyConfigServiceImpl::ProxyConfig::ManualProxy* proxy =
        init_config.MapSchemeToProxy(scheme);
    net::ProxyServer::Scheme net_scheme;
    if (scheme == "http" || scheme == "ftp")
      net_scheme = MK_SCHM(HTTP);
    else if (scheme == "https")
      net_scheme = MK_SCHM(HTTPS);
    else if (scheme == "socks")
      net_scheme = MK_SCHM(SOCKS4);
    SetManualProxy(MK_MODE(PROXY_PER_SCHEME), source, server_uri, net_scheme,
                   &init_config, proxy);
    CreateConfigService(init_config);

    ProxyConfigServiceImpl::ProxyConfig config;
    config_service()->UIGetProxyConfig(&config);

    // For owner, write access to config should be equal CanBeWrittenByOwner().
    // For non-owner, config is never writeable.
    bool expected_writeable_by_owner = CanBeWrittenByOwner(source);
    EXPECT_EQ(expected_writeable_by_owner,
              config.CanBeWrittenByUser(true, scheme));
    EXPECT_FALSE(config.CanBeWrittenByUser(false, scheme));

    const char* all_schemes[] = {
      "http", "https", "ftp", "socks",
    };

    // Rest of protos should be writeable by owner, but not writeable by
    // non-owner.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(all_schemes); ++i) {
      if (scheme == all_schemes[i])
        continue;
      EXPECT_TRUE(config.CanBeWrittenByUser(true, all_schemes[i]));
      EXPECT_FALSE(config.CanBeWrittenByUser(false, all_schemes[i]));
    }
  }

  // Synchronously gets the latest proxy config.
  bool SyncGetLatestProxyConfig(net::ProxyConfig* config) {
    // Let message loop process all messages.
    MessageLoop::current()->RunAllPending();
    // Calls IOGetProxyConfig (which is called from
    // ProxyConfigService::GetLatestProxyConfig), running on faked IO thread.
    return config_service_->IOGetProxyConfig(config);
  }

  ProxyConfigServiceImpl* config_service() const {
    return config_service_;
  }

 private:
  bool CanBeWrittenByOwner(
    ProxyConfigServiceImpl::ProxyConfig::Source source) const {
    return source == MK_SRC(POLICY) ? false : true;
  }

  ScopedStubCrosEnabler stub_cros_enabler_;
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  scoped_refptr<ProxyConfigServiceImpl> config_service_;
};

TEST_F(ProxyConfigServiceImplTest, ChromeosProxyConfigToNetProxyConfig) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));

    ProxyConfigServiceImpl::ProxyConfig init_config;
    InitConfigWithTestInput(tests[i].input, MK_SRC(OWNER), &init_config);
    CreateConfigService(init_config);

    net::ProxyConfig config;
    SyncGetLatestProxyConfig(&config);

    EXPECT_EQ(tests[i].auto_detect, config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(config.proxy_rules()));
  }
}

TEST_F(ProxyConfigServiceImplTest, ModifyFromUI) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));

    // Init with direct.
    ProxyConfigServiceImpl::ProxyConfig init_config;
    SetAutomaticProxy(MK_MODE(DIRECT), MK_SRC(OWNER), NULL, &init_config,
                      &init_config.automatic_proxy);
    CreateConfigService(init_config);

    // Set config to tests[i].input via UI.
    net::ProxyBypassRules bypass_rules;
    const Input& input = tests[i].input;
    switch (input.mode) {
      case MK_MODE(DIRECT) :
        config_service()->UISetProxyConfigToDirect();
        break;
      case MK_MODE(AUTO_DETECT) :
        config_service()->UISetProxyConfigToAutoDetect();
        break;
      case MK_MODE(PAC_SCRIPT) :
        config_service()->UISetProxyConfigToPACScript(GURL(input.pac_url));
        break;
      case MK_MODE(SINGLE_PROXY) :
        config_service()->UISetProxyConfigToSingleProxy(
            net::ProxyServer::FromURI(input.single_uri, MK_SCHM(HTTP)));
        if (input.bypass_rules) {
          bypass_rules.ParseFromStringUsingSuffixMatching(input.bypass_rules);
          config_service()->UISetProxyConfigBypassRules(bypass_rules);
        }
        break;
      case MK_MODE(PROXY_PER_SCHEME) :
        if (input.http_uri) {
          config_service()->UISetProxyConfigToProxyPerScheme("http",
                  net::ProxyServer::FromURI(input.http_uri, MK_SCHM(HTTP)));
        }
        if (input.https_uri) {
          config_service()->UISetProxyConfigToProxyPerScheme("https",
              net::ProxyServer::FromURI(input.https_uri, MK_SCHM(HTTPS)));
        }
        if (input.ftp_uri) {
          config_service()->UISetProxyConfigToProxyPerScheme("ftp",
              net::ProxyServer::FromURI(input.ftp_uri, MK_SCHM(HTTP)));
        }
        if (input.socks_uri) {
          config_service()->UISetProxyConfigToProxyPerScheme("socks",
              net::ProxyServer::FromURI(input.socks_uri, MK_SCHM(SOCKS5)));
        }
        if (input.bypass_rules) {
          bypass_rules.ParseFromStringUsingSuffixMatching(input.bypass_rules);
          config_service()->UISetProxyConfigBypassRules(bypass_rules);
        }
        break;
    }

    // Retrieve config from IO thread.
    net::ProxyConfig io_config;
    SyncGetLatestProxyConfig(&io_config);
    EXPECT_EQ(tests[i].auto_detect, io_config.auto_detect());
    EXPECT_EQ(tests[i].pac_url, io_config.pac_url());
    EXPECT_TRUE(tests[i].proxy_rules.Matches(io_config.proxy_rules()));

    // Retrieve config from UI thread.
    ProxyConfigServiceImpl::ProxyConfig ui_config;
    config_service()->UIGetProxyConfig(&ui_config);
    EXPECT_EQ(input.mode, ui_config.mode);
    if (tests[i].is_valid) {
      if (input.pac_url)
        EXPECT_EQ(GURL(input.pac_url), ui_config.automatic_proxy.pac_url);
      const net::ProxyRulesExpectation& proxy_rules = tests[i].proxy_rules;
      if (input.single_uri)
        EXPECT_EQ(proxy_rules.single_proxy,
                  ui_config.single_proxy.server.ToURI());
      if (input.http_uri)
        EXPECT_EQ(proxy_rules.proxy_for_http,
                  ui_config.http_proxy.server.ToURI());
      if (input.https_uri)
        EXPECT_EQ(proxy_rules.proxy_for_https,
                  ui_config.https_proxy.server.ToURI());
      if (input.ftp_uri)
        EXPECT_EQ(proxy_rules.proxy_for_ftp,
                  ui_config.ftp_proxy.server.ToURI());
      if (input.socks_uri) {
        EXPECT_EQ(proxy_rules.fallback_proxy,
                  ui_config.socks_proxy.server.ToURI());
      }
      if (input.bypass_rules)
        EXPECT_TRUE(bypass_rules.Equals(ui_config.bypass_rules));
    }
  }
}

TEST_F(ProxyConfigServiceImplTest, ProxyChangedObserver) {
  // This is used to observe for OnProxyConfigChanged notification.
  class ProxyChangedObserver : public net::ProxyConfigService::Observer {
   public:
    explicit ProxyChangedObserver(
        const scoped_refptr<ProxyConfigServiceImpl>& config_service)
        : config_service_(config_service) {
      config_service_->AddObserver(this);
    }
    virtual ~ProxyChangedObserver() {
      config_service_->RemoveObserver(this);
    }
    const net::ProxyConfig& config() const {
      return config_;
    }

   private:
    virtual void OnProxyConfigChanged(const net::ProxyConfig& config) {
      config_ = config;
    }

    scoped_refptr<ProxyConfigServiceImpl> config_service_;
    net::ProxyConfig config_;
  };

  // Init with direct.
  ProxyConfigServiceImpl::ProxyConfig init_config;
  SetAutomaticProxy(MK_MODE(DIRECT), MK_SRC(OWNER), NULL, &init_config,
                    &init_config.automatic_proxy);
  CreateConfigService(init_config);

  ProxyChangedObserver observer(config_service());

  // Set to pac script from UI.
  EXPECT_TRUE(config_service()->UISetProxyConfigToPACScript(
      GURL("http://wpad.dat")));
  // Retrieve config from IO thread.
  net::ProxyConfig io_config;
  SyncGetLatestProxyConfig(&io_config);

  // Observer should have gotten the same new proxy config.
  EXPECT_TRUE(io_config.Equals(observer.config()));
}

TEST_F(ProxyConfigServiceImplTest, SerializeAndDeserialize) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    if (!tests[i].is_valid)
      continue;

    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));

    ProxyConfigServiceImpl::ProxyConfig source_config;
    InitConfigWithTestInput(tests[i].input, MK_SRC(OWNER), &source_config);

    // Serialize source_config into std::string.
    std::string serialized_value;
    EXPECT_TRUE(source_config.Serialize(&serialized_value));

    // Deserialize std:string into target_config.
    ProxyConfigServiceImpl::ProxyConfig target_config;
    EXPECT_TRUE(target_config.Deserialize(serialized_value));

    // Compare the configs after serialization and deserialization.
    net::ProxyConfig net_src_cfg;
    net::ProxyConfig net_tgt_cfg;
    source_config.ToNetProxyConfig(&net_src_cfg);
    target_config.ToNetProxyConfig(&net_tgt_cfg);
#if !defined(NDEBUG)
    if (!net_src_cfg.Equals(net_tgt_cfg)) {
      std::string src_output, tgt_output;
      JSONStringValueSerializer src_serializer(&src_output);
      src_serializer.Serialize(*net_src_cfg.ToValue());
      JSONStringValueSerializer tgt_serializer(&tgt_output);
      tgt_serializer.Serialize(*net_tgt_cfg.ToValue());
      VLOG(1) << "source:\n" << src_output
              << "\ntarget:\n" << tgt_output;
    }
#endif  // !defined(NDEBUG)
    EXPECT_TRUE(net_src_cfg.Equals(net_tgt_cfg));
  }
}

TEST_F(ProxyConfigServiceImplTest, ReadWriteAccessForPolicySource) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    if (!tests[i].test_read_write_access)
      continue;
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));
    TestReadWriteAccessForMode(tests[i].input, MK_SRC(POLICY));
  }
}

TEST_F(ProxyConfigServiceImplTest, ReadWriteAccessForOwnerSource) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    if (!tests[i].test_read_write_access)
      continue;
    SCOPED_TRACE(StringPrintf("Test[%" PRIuS "] %s", i,
                              tests[i].description.c_str()));
    TestReadWriteAccessForMode(tests[i].input, MK_SRC(OWNER));
  }
}

TEST_F(ProxyConfigServiceImplTest, ReadWriteAccessForMixedSchemes) {
  const char* http_uri = "www.google.com:80";
  const char* https_uri = "www.foo.com:110";
  const char* ftp_uri = "ftp.foo.com:121";
  const char* socks_uri = "socks.com:888";

  // Init with policy source.
  TestReadWriteAccessForScheme(MK_SRC(POLICY), http_uri, "http");
  TestReadWriteAccessForScheme(MK_SRC(POLICY), https_uri, "https");
  TestReadWriteAccessForScheme(MK_SRC(POLICY), ftp_uri, "ftp");
  TestReadWriteAccessForScheme(MK_SRC(POLICY), socks_uri, "socks");

  // Init with owner source.
  TestReadWriteAccessForScheme(MK_SRC(OWNER), http_uri, "http");
  TestReadWriteAccessForScheme(MK_SRC(OWNER), https_uri, "https");
  TestReadWriteAccessForScheme(MK_SRC(OWNER), ftp_uri, "ftp");
  TestReadWriteAccessForScheme(MK_SRC(OWNER), socks_uri, "socks");
}

}  // namespace chromeos
