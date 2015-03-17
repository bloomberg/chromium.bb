// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_

#include <string>
#include <utility>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace net {
class HostPortPair;
class ProxyServer;
}

namespace data_reduction_proxy {

// Contains information about a given proxy server. |proxy_servers| contains
// the configured data reduction proxy servers. |is_fallback|, |is_alternative|
// and |is_ssl| note whether the given proxy is a fallback, an alternative,
// or a proxy for ssl; these are not mutually exclusive.
struct DataReductionProxyTypeInfo {
  DataReductionProxyTypeInfo();
  ~DataReductionProxyTypeInfo();
  std::pair<net::ProxyServer, net::ProxyServer> proxy_servers;
  bool is_fallback;
  bool is_alternative;
  bool is_ssl;
};

// Provides initialization parameters. Proxy origins, and the secure proxy
// check url are are taken from flags if available and from preprocessor
// constants otherwise. The DataReductionProxySettings class and others use this
// class to determine the necessary DNS names to configure use of the Data
// Reduction Proxy.
class DataReductionProxyParams : public DataReductionProxyConfigValues {
 public:
  // Flags used during construction that specify if the data reduction proxy
  // is allowed to be used, if the fallback proxy is allowed to be used, if
  // an alternative set of proxies is allowed to be used, if the promotion is
  // allowed to be shown, and if this instance is part of a holdback experiment.
  static const unsigned int kAllowed = (1 << 0);
  static const unsigned int kFallbackAllowed = (1 << 1);
  static const unsigned int kAlternativeAllowed = (1 << 2);
  static const unsigned int kAlternativeFallbackAllowed = (1 << 3);
  static const unsigned int kAllowAllProxyConfigurations = kAllowed |
      kFallbackAllowed | kAlternativeAllowed | kAlternativeFallbackAllowed;
  static const unsigned int kPromoAllowed = (1 << 4);
  static const unsigned int kHoldback = (1 << 5);

  // Returns true if this client is part of field trial to use an alternative
  // configuration for the data reduction proxy.
  static bool IsIncludedInAlternativeFieldTrial();

  // Returns true if this client is part of the field trial that should display
  // a promotion for the data reduction proxy.
  static bool IsIncludedInPromoFieldTrial();

  // Returns true if this client is part of a field trial that bypasses the
  // proxy if the request resource type is on the critical path (e.g. HTML).
  static bool IsIncludedInCriticalPathBypassFieldTrial();

  // Returns true if this client is part of a field trial that runs a holdback
  // experiment. A holdback experiment is one in which a fraction of browser
  // instances will not be configured to use the data reduction proxy even if
  // users have enabled it to be used. The UI will not indicate that a holdback
  // is in effect.
  static bool IsIncludedInHoldbackFieldTrial();

  // Returns true if this client is part of a field trial that removes the
  // |MISSING_VIA_HEADER_OTHER| proxy bypass case. This experiment changes proxy
  // bypass logic to not trigger a proxy bypass when a response with a non-4xx
  // response code is expected to have a data reduction proxy via header, but
  // the data reduction proxy via header is missing.
  static bool IsIncludedInRemoveMissingViaHeaderOtherBypassFieldTrial();

  // Returns true if this client is part of a field trial that relaxes the
  // |MISSING_VIA_HEADER_OTHER| proxy bypass case. In this experiment, if a
  // response with a data reduction proxy via header has been received through
  // the proxy since the last network change, then don't bypass on missing via
  // headers in responses with non-4xx response codes.
  static bool IsIncludedInRelaxMissingViaHeaderOtherBypassFieldTrial();

  // Returns true if this client is part of the field trial that should display
  // a promotion for the data reduction proxy on Android One devices.
  static bool IsIncludedInAndroidOnePromoFieldTrial(
      const char* build_fingerprint);

  // Returns true if this client has the command line switch to enable Lo-Fi
  // mode.
  static bool IsLoFiEnabled();

  // Returns true if this client has the command line switch to show
  // interstitials for data reduction proxy bypasses.
  static bool WarnIfNoDataReductionProxy();

  // Returns true if the Data Reduction Proxy supports the scheme of the
  // provided |url|.
  static bool CanProxyURLScheme(const GURL& url);

  // Returns true if this client is part of a field trial that sets the origin
  // proxy server as quic://proxy.googlezip.net.
  static bool IsIncludedInQuicFieldTrial();

  static std::string GetQuicFieldTrialName();

  // Constructs configuration parameters. If |kAllowed|, then the standard
  // data reduction proxy configuration is allowed to be used. If
  // |kfallbackAllowed| a fallback proxy can be used if the primary proxy is
  // bypassed or disabled. If |kAlternativeAllowed| then an alternative proxy
  // configuration is allowed to be used. This alternative configuration would
  // replace the primary and fallback proxy configurations if enabled. Finally
  // if |kPromoAllowed|, the client may show a promotion for the data reduction
  // proxy.
  //
  // A standard configuration has a primary proxy, and a fallback proxy for
  // HTTP traffic. The alternative configuration has a different primary and
  // fallback proxy for HTTP traffic, and an SSL proxy.
  explicit DataReductionProxyParams(int flags);

  ~DataReductionProxyParams() override;

  // If true, uses QUIC instead of SPDY to connect to proxies that use TLS.
  void EnableQuic(bool enable);

  // Overrides of |DataReductionProxyConfigValues|
  bool UsingHTTPTunnel(const net::HostPortPair& proxy_server) const override;

  bool IsDataReductionProxy(
      const net::HostPortPair& host_port_pair,
      DataReductionProxyTypeInfo* proxy_info) const override;

  const net::ProxyServer& origin() const override;

  const net::ProxyServer& fallback_origin() const override;

  const net::ProxyServer& ssl_origin() const override;

  const net::ProxyServer& alt_origin() const override;

  const net::ProxyServer& alt_fallback_origin() const override;

  const GURL& secure_proxy_check_url() const override;

  bool allowed() const override;

  bool fallback_allowed() const override;

  bool alternative_allowed() const override;

  bool alternative_fallback_allowed() const override;

  bool promo_allowed() const override;

  bool holdback() const override;

 protected:
  // Test constructor that optionally won't call Init();
  DataReductionProxyParams(int flags,
                           bool should_call_init);

  // Initialize the values of the proxies, and secure proxy check URL, from
  // command line flags and preprocessor constants, and check that there are
  // corresponding definitions for the allowed configurations.
  bool Init(bool allowed,
            bool fallback_allowed,
            bool alt_allowed,
            bool alt_fallback_allowed);

  // Initialize the values of the proxies, and secure proxy check URL from
  // command line flags and preprocessor constants.
  void InitWithoutChecks();

  // Returns the corresponding string from preprocessor constants if defined,
  // and an empty string otherwise.
  virtual std::string GetDefaultDevOrigin() const;
  virtual std::string GetDefaultDevFallbackOrigin() const;
  virtual std::string GetDefaultOrigin() const;
  virtual std::string GetDefaultFallbackOrigin() const;
  virtual std::string GetDefaultSSLOrigin() const;
  virtual std::string GetDefaultAltOrigin() const;
  virtual std::string GetDefaultAltFallbackOrigin() const;
  virtual std::string GetDefaultSecureProxyCheckURL() const;
  virtual std::string GetDefaultWarmupURL() const;

  net::ProxyServer origin_;
  net::ProxyServer fallback_origin_;

 private:
  net::ProxyServer ssl_origin_;
  net::ProxyServer alt_origin_;
  net::ProxyServer alt_fallback_origin_;
  GURL secure_proxy_check_url_;
  GURL warmup_url_;

  bool allowed_;
  bool fallback_allowed_;
  bool alt_allowed_;
  bool alt_fallback_allowed_;
  bool promo_allowed_;
  bool holdback_;
  bool quic_enabled_;
  std::string override_quic_origin_;

  bool configured_on_command_line_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyParams);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_
