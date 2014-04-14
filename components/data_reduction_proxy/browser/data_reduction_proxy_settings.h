// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"

class PrefService;

namespace net {
class AuthChallengeInfo;
class HostPortPair;
class HttpAuthCache;
class HttpNetworkSession;
class HttpResponseHeaders;
class URLFetcher;
class URLRequestContextGetter;
}

namespace data_reduction_proxy {

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

COMPILE_ASSERT(kNumDaysInHistorySummary <= kNumDaysInHistory,
               DataReductionProxySettings_summary_too_long);

  // Values of the UMA DataReductionProxy.StartupState histogram.
  // This enum must remain synchronized with DataReductionProxyStartupState
  // in metrics/histograms/histograms.xml.
  enum ProxyStartupState {
    PROXY_NOT_AVAILABLE = 0,
    PROXY_DISABLED,
    PROXY_ENABLED,
    PROXY_STARTUP_STATE_COUNT,
  };

  // Values of the UMA DataReductionProxy.ProbeURL histogram.
  // This enum must remain synchronized with
  // DataReductionProxyProbeURLFetchResult in metrics/histograms/histograms.xml.
  // TODO(marq): Rename these histogram buckets with s/DISABLED/RESTRICTED/, so
  //     their names match the behavior they track.
  enum ProbeURLFetchResult {
    // The probe failed because the Internet was disconnected.
    INTERNET_DISCONNECTED = 0,

    // The probe failed for any other reason, and as a result, the proxy was
    // disabled.
    FAILED_PROXY_DISABLED,

    // The probe failed, but the proxy was already restricted.
    FAILED_PROXY_ALREADY_DISABLED,

    // THe probe succeeded, and as a result the proxy was restricted.
    SUCCEEDED_PROXY_ENABLED,

    // The probe succeeded, but the proxy was already restricted.
    SUCCEEDED_PROXY_ALREADY_ENABLED,

    // This must always be last.
    PROBE_URL_FETCH_RESULT_COUNT
  };

// Central point for configuring the data reduction proxy.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
// TODO(marq): Convert this to be a KeyedService with an
// associated factory class, and refactor the Java call sites accordingly.
class DataReductionProxySettings
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  typedef std::vector<long long> ContentLengthList;
  // TODO(marq): Consider instead using a std::pair instead of a vector.
  typedef std::vector<GURL> DataReductionProxyList;

  DataReductionProxySettings();
  virtual ~DataReductionProxySettings();

  // Initializes the data reduction proxy with profile and local state prefs,
  // and a |UrlRequestContextGetter| for canary probes. The caller must ensure
  // that all parameters remain alive for the lifetime of the
  // |DataReductionProxySettings| instance.
  void InitDataReductionProxySettings(
      PrefService* prefs,
      PrefService* local_state_prefs,
      net::URLRequestContextGetter* url_request_context_getter,
      scoped_ptr<DataReductionProxyConfigurator> config);

  // If proxy authentication is compiled in, pre-cache authentication
  // keys for all configured proxies in |session|.
  static void InitDataReductionProxySession(net::HttpNetworkSession* session);

  // Returns true if the data reduction proxy is allowed to be used on this
  // instance of Chrome. This could return false, for example, if this instance
  // is not part of the field trial, or if the proxy name is not configured
  // via gyp.
  static bool IsDataReductionProxyAllowed();

  // Returns true if a screen promoting the data reduction proxy is allowed to
  // be shown. Logic that decides when to show the promo should check its
  // availability. This would return false if not part of a separate field
  // trial that governs the use of the promotion.
  static bool IsDataReductionProxyPromoAllowed();

  // Returns true if preconnect advisory hinting is enabled by command line
  // flag or Finch trial.
  static bool IsPreconnectHintingAllowed();

  // Returns the URL of the data reduction proxy.
  static std::string GetDataReductionProxyOrigin();

  // Returns the URL of the fallback data reduction proxy.
  static std::string GetDataReductionProxyFallback();

  // Returns a vector of GURLs for all configured proxies.
  static DataReductionProxyList GetDataReductionProxies();

  // Returns true if |auth_info| represents an authentication challenge from
  // a compatible, configured proxy.
  bool IsAcceptableAuthChallenge(net::AuthChallengeInfo* auth_info);

  // Returns a UTF16 string suitable for use as an authentication token in
  // response to the challenge represented by |auth_info|. If the token can't
  // be correctly generated for |auth_info|, returns an empty UTF16 string.
  base::string16 GetTokenForAuthChallenge(net::AuthChallengeInfo* auth_info);

  // Returns true if the proxy is enabled.
  bool IsDataReductionProxyEnabled();

  // Returns true if the proxy is managed by an adminstrator's policy.
  bool IsDataReductionProxyManaged();

  // Enables or disables the data reduction proxy. If a probe URL is available,
  // and a probe request fails at some point, the proxy won't be used until a
  // probe succeeds.
  void SetDataReductionProxyEnabled(bool enabled);

  // Returns the time in microseconds that the last update was made to the
  // daily original and received content lengths.
  int64 GetDataReductionLastUpdateTime();

  // Returns a vector containing the total size of all HTTP content that was
  // received over the last |kNumDaysInHistory| before any compression by the
  // data reduction proxy. Each element in the vector contains one day of data.
  ContentLengthList GetDailyOriginalContentLengths();

  // Returns an vector containing the aggregate received HTTP content in the
  // last |kNumDaysInHistory| days.
  ContentLengthList GetDailyReceivedContentLengths();

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  void InitPrefMembers();

  virtual net::URLFetcher* GetURLFetcher();

  // Virtualized for unit test support.
  virtual PrefService* GetOriginalProfilePrefs();
  virtual PrefService* GetLocalStatePrefs();

  void GetContentLengths(unsigned int days,
                         int64* original_content_length,
                         int64* received_content_length,
                         int64* last_update_time);
  ContentLengthList GetDailyContentLengths(const char* pref_name);

  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled|. If |restricted| is true, only enable the fallback
  // proxy. |at_startup| is true when this method is called from
  // InitDataReductionProxySettings.
  virtual void SetProxyConfigs(bool enabled, bool restricted, bool at_startup);

  // Metrics methods. Subclasses should override if they wish to provide
  // alternate methods.
  virtual void RecordDataReductionInit();

  virtual void AddDefaultProxyBypassRules();

  // Writes a warning to the log that is used in backend processing of
  // customer feedback. Virtual so tests can mock it for verification.
  virtual void LogProxyState(bool enabled, bool restricted, bool at_startup);

  // Virtualized for mocking
  virtual void RecordProbeURLFetchResult(
      data_reduction_proxy::ProbeURLFetchResult result);
  virtual void RecordStartupState(
      data_reduction_proxy::ProxyStartupState state);

  DataReductionProxyConfigurator* config() {
    return config_.get();
  }

 private:
  friend class DataReductionProxySettingsTestBase;
  friend class DataReductionProxySettingsTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestAuthenticationInit);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestAuthHashGeneration);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestAuthHashGenerationWithOriginSetViaSwitch);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestResetDataReductionStatistics);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestIsProxyEnabledOrManaged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestGetDailyContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestMaybeActivateDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestOnProxyEnabledPrefChange);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOn);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestInitDataReductionProxyOff);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestBypassList);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           CheckInitMetricsWhenNotAllowed);

  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  // Underlying implementation of InitDataReductionProxySession(), factored
  // out to be testable without creating a full HttpNetworkSession.
  static void InitDataReductionAuthentication(net::HttpAuthCache* auth_cache);

  void OnProxyEnabledPrefChange();

  void ResetDataReductionStatistics();

  void MaybeActivateDataReductionProxy(bool at_startup);

  // Requests the proxy probe URL, if one is set.  If unable to do so, disables
  // the proxy, if enabled. Otherwise enables the proxy if disabled by a probe
  // failure.
  void ProbeWhetherDataReductionProxyIsAvailable();
  std::string GetProxyCheckURL();

  // Returns a UTF16 string that's the hash of the configured authentication
  // key and |salt|. Returns an empty UTF16 string if no key is configured or
  // the data reduction proxy feature isn't available.
  static base::string16 AuthHashForSalt(int64 salt);

  bool restricted_by_carrier_;
  bool enabled_by_user_;

  scoped_ptr<net::URLFetcher> fetcher_;
  BooleanPrefMember spdy_proxy_auth_enabled_;

  PrefService* prefs_;
  PrefService* local_state_prefs_;

  net::URLRequestContextGetter* url_request_context_getter_;

  scoped_ptr<DataReductionProxyConfigurator> config_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettings);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_H_
