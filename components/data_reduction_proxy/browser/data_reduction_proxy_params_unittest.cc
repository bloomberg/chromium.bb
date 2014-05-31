// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Test values to replacethe values specified in preprocessor defines.
static const char kDefaultKey[] = "test-key";
static const char kDefaultDevOrigin[] = "https://dev.net:443/";
static const char kDefaultOrigin[] = "https://origin.net:443/";
static const char kDefaultFallbackOrigin[] = "http://fallback.net:80/";
static const char kDefaultSSLOrigin[] = "http://ssl.net:1080/";
static const char kDefaultAltOrigin[] = "https://alt.net:443/";
static const char kDefaultAltFallbackOrigin[] = "http://altfallback.net:80/";
static const char kDefaultProbeURL[] = "http://probe.net/";

static const char kFlagKey[] = "test-flag-key";
static const char kFlagOrigin[] = "https://origin.org:443/";
static const char kFlagFallbackOrigin[] = "http://fallback.org:80/";
static const char kFlagSSLOrigin[] = "http://ssl.org:1080/";
static const char kFlagAltOrigin[] = "https://alt.org:443/";
static const char kFlagAltFallbackOrigin[] = "http://altfallback.org:80/";
static const char kFlagProbeURL[] = "http://probe.org/";

// Used to emulate having constants defined by the preprocessor.
static const unsigned int HAS_NOTHING = 0x0;
static const unsigned int HAS_KEY = 0x1;
static const unsigned int HAS_DEV_ORIGIN = 0x2;
static const unsigned int HAS_ORIGIN = 0x4;
static const unsigned int HAS_FALLBACK_ORIGIN = 0x8;
static const unsigned int HAS_SSL_ORIGIN = 0x10;
static const unsigned int HAS_ALT_ORIGIN = 0x20;
static const unsigned int HAS_ALT_FALLBACK_ORIGIN = 0x40;
static const unsigned int HAS_PROBE_URL = 0x80;
static const unsigned int HAS_EVERYTHING = 0xff;
}  // namespace

namespace data_reduction_proxy {
class TestDataReductionProxyParams : public DataReductionProxyParams {
 public:

  TestDataReductionProxyParams(int flags,
                               unsigned int has_definitions)
      : DataReductionProxyParams(flags,
                                 false),
        has_definitions_(has_definitions) {
    init_result_ = Init(flags & DataReductionProxyParams::kAllowed,
                        flags & DataReductionProxyParams::kFallbackAllowed,
                        flags & DataReductionProxyParams::kAlternativeAllowed);
  }

  bool init_result() const {
    return init_result_;
  }

 protected:
  virtual std::string GetDefaultKey() const OVERRIDE {
    return GetDefinition(HAS_KEY, kDefaultKey);
  }

  virtual std::string GetDefaultDevOrigin() const OVERRIDE {
    return GetDefinition(HAS_DEV_ORIGIN, kDefaultDevOrigin);
  }

  virtual std::string GetDefaultOrigin() const OVERRIDE {
    return GetDefinition(HAS_ORIGIN, kDefaultOrigin);
  }

  virtual std::string GetDefaultFallbackOrigin() const OVERRIDE {
    return GetDefinition(HAS_FALLBACK_ORIGIN, kDefaultFallbackOrigin);
  }

  virtual std::string GetDefaultSSLOrigin() const OVERRIDE {
    return GetDefinition(HAS_SSL_ORIGIN, kDefaultSSLOrigin);
  }

  virtual std::string GetDefaultAltOrigin() const OVERRIDE {
    return GetDefinition(HAS_ALT_ORIGIN, kDefaultAltOrigin);
  }

  virtual std::string GetDefaultAltFallbackOrigin() const OVERRIDE {
    return GetDefinition(HAS_ALT_FALLBACK_ORIGIN, kDefaultAltFallbackOrigin);
  }

  virtual std::string GetDefaultProbeURL() const OVERRIDE {
    return GetDefinition(HAS_PROBE_URL, kDefaultProbeURL);
  }

 private:
  std::string GetDefinition(unsigned int has_def,
                            const std::string& definition) const {
    return ((has_definitions_ & has_def) ? definition : std::string());
  }

  unsigned int has_definitions_;
  bool init_result_;
};

class DataReductionProxyParamsTest : public testing::Test {
 public:
  void CheckParams(const TestDataReductionProxyParams& params,
                   bool expected_init_result,
                   bool expected_allowed,
                   bool expected_fallback_allowed,
                   bool expected_alternative_allowed,
                   bool expected_promo_allowed) {
    EXPECT_EQ(expected_init_result, params.init_result());
    EXPECT_EQ(expected_allowed, params.allowed());
    EXPECT_EQ(expected_fallback_allowed, params.fallback_allowed());
    EXPECT_EQ(expected_alternative_allowed, params.alternative_allowed());
    EXPECT_EQ(expected_promo_allowed, params.promo_allowed());
  }
  void CheckValues(const TestDataReductionProxyParams& params,
                   const std::string expected_key,
                   const std::string& expected_origin,
                   const std::string& expected_fallback_origin,
                   const std::string& expected_ssl_origin,
                   const std::string& expected_alt_origin,
                   const std::string& expected_alt_fallback_origin,
                   const std::string& expected_probe_url) {
    EXPECT_EQ(expected_key, params.key());
    EXPECT_EQ(GURL(expected_origin), params.origin());
    EXPECT_EQ(GURL(expected_fallback_origin), params.fallback_origin());
    EXPECT_EQ(GURL(expected_ssl_origin), params.ssl_origin());
    EXPECT_EQ(GURL(expected_alt_origin), params.alt_origin());
    EXPECT_EQ(GURL(expected_alt_fallback_origin), params.alt_fallback_origin());
    EXPECT_EQ(GURL(expected_probe_url), params.probe_url());
  }
};

TEST_F(DataReductionProxyParamsTest, EverythingDefined) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed, HAS_EVERYTHING);
  CheckParams(params, true, true, true, false, true);
  CheckValues(params,
              kDefaultKey,
              kDefaultDevOrigin,
              kDefaultFallbackOrigin,
              kDefaultSSLOrigin,
              kDefaultAltOrigin,
              kDefaultAltFallbackOrigin,
              kDefaultProbeURL);
}

TEST_F(DataReductionProxyParamsTest, NoDevOrigin) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      HAS_EVERYTHING & ~HAS_DEV_ORIGIN);
  CheckParams(params, true, true, true, false, true);
  CheckValues(params,
              kDefaultKey,
              kDefaultOrigin,
              kDefaultFallbackOrigin,
              kDefaultSSLOrigin,
              kDefaultAltOrigin,
              kDefaultAltFallbackOrigin,
              kDefaultProbeURL);
}

TEST_F(DataReductionProxyParamsTest, Flags) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyKey, kFlagKey);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy, kFlagOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback, kFlagFallbackOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy, kFlagSSLOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt, kFlagAltOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback, kFlagAltFallbackOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyProbeURL, kFlagProbeURL);
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kAlternativeAllowed |
      DataReductionProxyParams::kPromoAllowed, HAS_EVERYTHING);
  CheckParams(params, true, true, true, true, true);
  CheckValues(params,
              kFlagKey,
              kFlagOrigin,
              kFlagFallbackOrigin,
              kFlagSSLOrigin,
              kFlagAltOrigin,
              kFlagAltFallbackOrigin,
              kFlagProbeURL);
}

TEST_F(DataReductionProxyParamsTest, FlagsNoKey) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy, kFlagOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback, kFlagFallbackOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy, kFlagSSLOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt, kFlagAltOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback, kFlagAltFallbackOrigin);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyProbeURL, kFlagProbeURL);
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kAlternativeAllowed |
      DataReductionProxyParams::kPromoAllowed, HAS_EVERYTHING);
  EXPECT_FALSE(params.init_result());
}

TEST_F(DataReductionProxyParamsTest, InvalidConfigurations) {
  const struct {
    bool allowed;
    bool fallback_allowed;
    bool alternative_allowed;
    bool promo_allowed;
    unsigned int missing_definitions;
    bool expected_result;
  } tests[]  = {
    { true, true, true, true, HAS_NOTHING, true },
    { true, true, true, true, HAS_KEY, false },
    { true, true, true, true, HAS_DEV_ORIGIN, true },
    { true, true, true, true, HAS_ORIGIN, true },
    { true, true, true, true, HAS_ORIGIN | HAS_DEV_ORIGIN, false },
    { true, true, true, true, HAS_FALLBACK_ORIGIN, false },
    { true, true, true, true, HAS_SSL_ORIGIN, false },
    { true, true, true, true, HAS_ALT_ORIGIN, false },
    { true, true, true, true, HAS_ALT_FALLBACK_ORIGIN, false },
    { true, true, true, true, HAS_PROBE_URL, false },

    { true, false, true, true, HAS_NOTHING, true },
    { true, false, true, true, HAS_KEY, false },
    { true, false, true, true, HAS_ORIGIN | HAS_DEV_ORIGIN, false },
    { true, false, true, true, HAS_FALLBACK_ORIGIN, true },
    { true, false, true, true, HAS_SSL_ORIGIN, false },
    { true, false, true, true, HAS_ALT_ORIGIN, false },
    { true, false, true, true, HAS_ALT_FALLBACK_ORIGIN, true },
    { true, false, true, true, HAS_PROBE_URL, false },

    { true, true, false, true, HAS_NOTHING, true },
    { true, true, false, true, HAS_KEY, false },
    { true, true, false, true, HAS_ORIGIN | HAS_DEV_ORIGIN, false },
    { true, true, false, true, HAS_FALLBACK_ORIGIN, false },
    { true, true, false, true, HAS_SSL_ORIGIN, true },
    { true, true, false, true, HAS_ALT_ORIGIN, true },
    { true, true, false, true, HAS_ALT_FALLBACK_ORIGIN, true },
    { true, true, false, true, HAS_PROBE_URL, false },

    { true, false, false, true, HAS_KEY, false },
    { true, false, false, true, HAS_ORIGIN | HAS_DEV_ORIGIN, false },
    { true, false, false, true, HAS_FALLBACK_ORIGIN, true },
    { true, false, false, true, HAS_SSL_ORIGIN, true },
    { true, false, false, true, HAS_ALT_ORIGIN, true },
    { true, false, false, true, HAS_ALT_FALLBACK_ORIGIN, true },
    { true, false, false, true, HAS_PROBE_URL, false },

    { false, true, true, true, HAS_NOTHING, false },
    { false, true, true, true, HAS_KEY, false },
    { false, true, true, true, HAS_ORIGIN | HAS_DEV_ORIGIN, false },
    { false, true, true, true, HAS_FALLBACK_ORIGIN, false },
    { false, true, true, true, HAS_SSL_ORIGIN, false },
    { false, true, true, true, HAS_ALT_ORIGIN, false },
    { false, true, true, true, HAS_ALT_FALLBACK_ORIGIN, false },
    { false, true, true, true, HAS_PROBE_URL, false },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (tests[i].alternative_allowed)
      flags |= DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].promo_allowed)
      flags |= DataReductionProxyParams::kPromoAllowed;
    TestDataReductionProxyParams params(
        flags,
        HAS_EVERYTHING & ~(tests[i].missing_definitions));
    EXPECT_EQ(tests[i].expected_result, params.init_result());
  }
}

}  // namespace data_reduction_proxy
