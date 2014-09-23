// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_retry_info.h"
#include "url/gurl.h"

namespace base {
class TimeDelta;
}

namespace net {
class ProxyConfig;
class URLRequest;
}

namespace data_reduction_proxy {

// Contains information about a given proxy server. |proxy_servers| contains
// the configured data reduction proxy servers. |is_fallback|, |is_alternative|
// and |is_ssl| note whether the given proxy is a fallback, an alternative,
// or a proxy for ssl; these are not mutually exclusive.
struct DataReductionProxyTypeInfo {
  DataReductionProxyTypeInfo();
  ~DataReductionProxyTypeInfo();
  std::pair<GURL, GURL> proxy_servers;
  bool is_fallback;
  bool is_alternative;
  bool is_ssl;
};

// Provides initialization parameters. Proxy origins, and the probe url are
// are taken from flags if available and from preprocessor constants otherwise.
// The DataReductionProxySettings class and others use this class to determine
// the necessary DNS names to configure use of the data reduction proxy.
class DataReductionProxyParams {
 public:
  // Flags used during construction that specify if the data reduction proxy
  // is allowed to be used, if the fallback proxy is allowed to be used, if
  // an alternative set of proxies is allowed to be used, if the promotion is
  // allowed to be shown, and if this instance is part of a holdback experiment.
  static const unsigned int kAllowed = (1 << 0);
  static const unsigned int kFallbackAllowed = (1 << 1);
  static const unsigned int kAlternativeAllowed = (1 << 2);
  static const unsigned int kAlternativeFallbackAllowed = (1 << 3);
  static const unsigned int kPromoAllowed = (1 << 4);
  static const unsigned int kHoldback = (1 << 5);

  typedef std::vector<GURL> DataReductionProxyList;

  // Returns true if this client is part of field trial to use an alternative
  // configuration for the data reduction proxy.
  static bool IsIncludedInAlternativeFieldTrial();

  // Returns true if this client is part of the field trial that should display
  // a promotion for the data reduction proxy.
  static bool IsIncludedInPromoFieldTrial();

  // Returns true if this client is part of a field trial that uses preconnect
  // hinting.
  static bool IsIncludedInPreconnectHintingFieldTrial();

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

  // Creates a copy of the configuration parameters.
  scoped_ptr<DataReductionProxyParams> Clone();

  virtual ~DataReductionProxyParams();

  // Returns true if a data reduction proxy was used for the given |request|.
  // If true, |proxy_info.proxy_servers.first| will contain the name of the
  // proxy that was used. |proxy_info.proxy_servers.second| will contain the
  // name of the data reduction proxy server that would be used if
  // |proxy_info.proxy_server.first| is bypassed, if one exists. In addition,
  // |proxy_info| will note if the proxy used was a fallback, an alternative,
  // or a proxy for ssl; these are not mutually exclusive. |proxy_info| can be
  // NULL if the caller isn't interested in its values.
  virtual bool WasDataReductionProxyUsed(
      const net::URLRequest* request,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if the specified |host_port_pair| matches a data reduction
  // proxy. If true, |proxy_info.proxy_servers.first| will contain the name of
  // the proxy that matches. |proxy_info.proxy_servers.second| will contain the
  // name of the data reduction proxy server that would be used if
  // |proxy_info.proxy_server.first| is bypassed, if one exists. In addition,
  // |proxy_info| will note if the proxy was a fallback, an alternative, or a
  // proxy for ssl; these are not mutually exclusive. |proxy_info| can be NULL
  // if the caller isn't interested in its values. Virtual for testing.
  virtual bool IsDataReductionProxy(
      const net::HostPortPair& host_port_pair,
      DataReductionProxyTypeInfo* proxy_info) const;

  // Returns true if this request would be bypassed by the data request proxy
  // based on applying the |data_reduction_proxy_config| param rules to the
  // request URL.
  bool IsBypassedByDataReductionProxyLocalRules(
      const net::URLRequest& request,
      const net::ProxyConfig& data_reduction_proxy_config) const;

  // Checks if all configured data reduction proxies are in the retry map.
  // Returns true if the request is bypassed by all configured data reduction
  // proxies and returns the bypass delay in delay_seconds (if not NULL). If
  // there are no configured data reduction proxies, returns false. If
  // the request is bypassed by more than one proxy, delay_seconds returns
  // the shortest delay.
  bool AreDataReductionProxiesBypassed(const net::URLRequest& request,
                                       base::TimeDelta* min_retry_delay) const;

  // Checks if all configured data reduction proxies are in the retry map.
  // Returns true if the request is bypassed by all configured data reduction
  // proxies and returns the bypass delay in delay_seconds (if not NULL). If
  // there are no configured data reduction proxies, returns false. If
  // the request is bypassed by more than one proxy, delay_seconds returns
  // the shortest delay.
  bool AreProxiesBypassed(const net::ProxyRetryInfoMap& retry_map,
                          bool is_https,
                          base::TimeDelta* min_retry_delay) const;

  // Returns the data reduction proxy primary origin.
  const GURL& origin() const {
    return origin_;
  }

  // Returns the data reduction proxy fallback origin.
  const GURL& fallback_origin() const {
    return fallback_origin_;
  }

  // Returns the data reduction proxy ssl origin that is used with the
  // alternative proxy configuration.
  const GURL& ssl_origin() const {
    return ssl_origin_;
  }

  // Returns the alternative data reduction proxy primary origin.
  const GURL& alt_origin() const {
    return alt_origin_;
  }

  // Returns the alternative data reduction proxy fallback origin.
  const GURL& alt_fallback_origin() const {
    return alt_fallback_origin_;
  }

  // Returns the URL to probe to decide if the primary origin should be used.
  const GURL& probe_url() const {
    return probe_url_;
  }

  // Returns the URL to fetch to warm the data reduction proxy connection.
  const GURL& warmup_url() const {
    return warmup_url_;
  }

  // Returns true if the data reduction proxy configuration may be used.
  bool allowed() const {
    return allowed_;
  }

  // Returns true if the fallback proxy may be used.
  bool fallback_allowed() const {
    return fallback_allowed_;
  }

  // Returns true if the alternative data reduction proxy configuration may be
  // used.
  bool alternative_allowed() const {
    return alt_allowed_;
  }

  // Returns true if the alternative fallback data reduction proxy
  // configuration may be used.
  bool alternative_fallback_allowed() const {
    return alt_fallback_allowed_;
  }

  // Returns true if the data reduction proxy promo may be shown.
  // This is idependent of whether the data reduction proxy is allowed.
  // TODO(bengr): maybe tie to whether proxy is allowed.
  bool promo_allowed() const {
    return promo_allowed_;
  }

  // Returns true if the data reduction proxy should not actually use the
  // proxy if enabled.
  bool holdback() const {
    return holdback_;
  }

  // Given |allowed_|, |fallback_allowed_|, and |alt_allowed_|, returns the
  // list of data reduction proxies that may be used.
  DataReductionProxyList GetAllowedProxies() const;

  // Returns true if any proxy origins are set on the command line.
  bool is_configured_on_command_line() const {
    return configured_on_command_line_;
  }

 protected:
  // Test constructor that optionally won't call Init();
  DataReductionProxyParams(int flags,
                           bool should_call_init);

  DataReductionProxyParams(const DataReductionProxyParams& params);

  // Initialize the values of the proxies, and probe URL, from command
  // line flags and preprocessor constants, and check that there are
  // corresponding definitions for the allowed configurations.
  bool Init(bool allowed,
            bool fallback_allowed,
            bool alt_allowed,
            bool alt_fallback_allowed);

  // Initialize the values of the proxies, and probe URL from command
  // line flags and preprocessor constants.
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
  virtual std::string GetDefaultProbeURL() const;
  virtual std::string GetDefaultWarmupURL() const;

 private:
  // Checks if the primary and fallback data reduction proxies are in the retry
  // map. Returns true if the request is bypassed by both data reduction
  // proxies and returns the shortest bypass delay in delay_seconds (if not
  // NULL). If the fallback proxy is not valid, returns true if primary proxy
  // was bypassed and returns its bypass delay.
  bool ArePrimaryAndFallbackBypassed(const net::ProxyRetryInfoMap& retry_map,
                                     const GURL& primary,
                                     const GURL& fallback,
                                     base::TimeDelta* min_retry_delay) const;

  DataReductionProxyParams& operator=(const DataReductionProxyParams& params);

  GURL origin_;
  GURL fallback_origin_;
  GURL ssl_origin_;
  GURL alt_origin_;
  GURL alt_fallback_origin_;
  GURL probe_url_;
  GURL warmup_url_;

  bool allowed_;
  bool fallback_allowed_;
  bool alt_allowed_;
  bool alt_fallback_allowed_;
  bool promo_allowed_;
  bool holdback_;

  bool configured_on_command_line_;
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_PARAMS_H_
